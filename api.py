import os
import shutil
import asyncio
import tempfile
import logging
from typing import Optional
from fastapi import FastAPI, UploadFile, File, HTTPException, BackgroundTasks
from fastapi.responses import FileResponse
from pydantic import BaseModel

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("skp2gltf-api")

app = FastAPI(title="skp2gltf Pipeline API Service")
lock = asyncio.Lock()


class ConvertRequest(BaseModel):
    input_path: str
    output_dir: Optional[str] = "/work/output"
    output_name: Optional[str] = None
    format: Optional[str] = "glb"
    draco: Optional[bool] = False
    draco_speed: Optional[int] = 5
    draco_pos_bits: Optional[int] = 14
    draco_tex_bits: Optional[int] = 12
    draco_norm_bits: Optional[int] = 10
    tex_res: Optional[int] = 1024
    ktx2: Optional[bool] = False
    ktx2_quality: Optional[int] = 128
    ktx2_uastc: Optional[bool] = True


def cleanup_tmp(path: str):
    if os.path.exists(path):
        if os.path.isdir(path):
            shutil.rmtree(path)
        else:
            os.remove(path)
        logger.info(f"Cleaned up {path}")


async def run_conversion(
    input_path: str,
    output_dir: str,
    output_name: str,
    format: str,
    draco: bool = False,
    draco_speed: int = 5,
    draco_pos_bits: int = 14,
    draco_tex_bits: int = 12,
    draco_norm_bits: int = 10,
    tex_res: int = 1024,
    ktx2: bool = False,
    ktx2_quality: int = 128,
    ktx2_uastc: bool = True,
):
    arch = os.uname().machine
    is_arm64 = arch in ["aarch64", "arm64"]

    cmd = []
    if is_arm64 and shutil.which("box64"):
        cmd += ["box64"]

    wine_bin = "wine64" if shutil.which("wine64") else "wine"
    exe_path = "/app/skp2gltf.exe"

    if not os.path.exists(exe_path):
        exe_path = os.getenv("SKP2GLTF_EXE", "/app/skp2gltf.exe")

    cmd += [wine_bin, exe_path, input_path, output_dir, output_name, format]

    if draco:
        cmd.append("draco")
        cmd.append(f"draco-speed:{draco_speed}")
        cmd.append(f"draco-pos:{draco_pos_bits}")
        cmd.append(f"draco-tex:{draco_tex_bits}")
        cmd.append(f"draco-norm:{draco_norm_bits}")

    cmd.append(f"tex-res:{tex_res}")

    if ktx2:
        cmd.append("ktx2")
        cmd.append(f"ktx2-quality:{ktx2_quality}")
        if ktx2_uastc:
            cmd.append("ktx2-uastc")

    logger.info(f"Executing: {' '.join(cmd)}")

    env = os.environ.copy()
    if "DISPLAY" not in env:
        env["DISPLAY"] = ":99"

    process = await asyncio.create_subprocess_exec(
        *cmd, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE, env=env
    )

    stdout, stderr = await process.communicate()

    if process.returncode != 0:
        err_msg = stderr.decode()
        logger.error(f"Conversion failed (code {process.returncode}): {err_msg}")
        return False, err_msg

    return True, stdout.decode()


@app.get("/health")
async def health():
    xvfb_active = os.path.exists("/tmp/.X99-lock")
    logger.info(f"Health check hit: xvfb_active={xvfb_active}")
    return {
        "status": "ready",
        "architecture": os.uname().machine,
        "xvfb_active": xvfb_active,
    }


@app.post("/convert")
async def convert_upload(
    background_tasks: BackgroundTasks,
    file: UploadFile = File(...),
    format: str = "glb",
    draco: bool = False,
    draco_speed: int = 5,
    draco_pos_bits: int = 14,
    draco_tex_bits: int = 12,
    draco_norm_bits: int = 10,
    tex_res: int = 1024,
    ktx2: bool = False,
    ktx2_quality: int = 128,
    ktx2_uastc: bool = True,
):
    if not file.filename.lower().endswith(".skp"):
        raise HTTPException(status_code=400, detail="Only .skp files supported")
    if format not in ["glb", "gltf"]:
        raise HTTPException(status_code=400, detail="Invalid format")

    tmp_dir = tempfile.mkdtemp(prefix="api_skp_")
    input_path = os.path.join(tmp_dir, file.filename)
    output_name = os.path.splitext(file.filename)[0]
    out_ext = ".glb" if format == "glb" else ".gltf"
    output_file = os.path.join(tmp_dir, output_name + out_ext)

    try:
        with open(input_path, "wb") as buffer:
            shutil.copyfileobj(file.file, buffer)

        async with lock:
            ok, msg = await run_conversion(
                input_path,
                tmp_dir,
                output_name,
                format,
                draco,
                draco_speed,
                draco_pos_bits,
                draco_tex_bits,
                draco_norm_bits,
                tex_res,
                ktx2,
                ktx2_quality,
                ktx2_uastc,
            )
            if not ok:
                raise HTTPException(status_code=500, detail=f"Conversion failed: {msg}")

        if not os.path.exists(output_file):
            raise HTTPException(
                status_code=500, detail="Output file not found after conversion"
            )

        background_tasks.add_task(cleanup_tmp, tmp_dir)
        return FileResponse(output_file, filename=output_name + out_ext)
    except Exception as e:
        cleanup_tmp(tmp_dir)
        if isinstance(e, HTTPException):
            raise e
        logger.exception("Conversion task failed")
        raise HTTPException(status_code=500, detail=str(e))


@app.post("/convert-path")
async def convert_path(req: ConvertRequest):
    if not os.path.exists(req.input_path):
        raise HTTPException(status_code=404, detail="Input file not found")

    out_name = req.output_name or os.path.splitext(os.path.basename(req.input_path))[0]
    os.makedirs(req.output_dir, exist_ok=True)

    async with lock:
        ok, msg = await run_conversion(
            req.input_path,
            req.output_dir,
            out_name,
            req.format,
            req.draco,
            req.draco_speed,
            req.draco_pos_bits,
            req.draco_tex_bits,
            req.draco_norm_bits,
            req.tex_res,
            req.ktx2,
            req.ktx2_quality,
            req.ktx2_uastc,
        )
        if not ok:
            raise HTTPException(status_code=500, detail=f"Conversion failed: {msg}")

    return {"status": "success", "dest": f"{req.output_dir}/{out_name}.{req.format}"}


if __name__ == "__main__":
    import uvicorn

    uvicorn.run(app, host="0.0.0.0", port=8000)

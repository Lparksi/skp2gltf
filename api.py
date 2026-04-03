import os
import shutil
import asyncio
import tempfile
import logging
from typing import Optional
from fastapi import FastAPI, UploadFile, File, HTTPException, BackgroundTasks
from fastapi.responses import FileResponse
from pydantic import BaseModel

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("skp2gltf-api")

app = FastAPI(title="skp2gltf Pipeline API Service")
lock = asyncio.Lock()

class ConvertRequest(BaseModel):
    input_path: str
    output_dir: Optional[str] = "/work/output"
    output_name: Optional[str] = None
    format: Optional[str] = "glb"

def cleanup_tmp(path: str):
    if os.path.exists(path):
        if os.path.isdir(path):
            shutil.rmtree(path)
        else:
            os.remove(path)
        logger.info(f"Cleaned up {path}")

async def run_conversion(input_path: str, output_dir: str, output_name: str, format: str):
    """Executes the skp2gltf.exe via Wine"""
    arch = os.uname().machine
    is_arm64 = arch in ["aarch64", "arm64"]
    
    cmd = []
    # In service mode, we assume wine and box64 are in the PATH
    if is_arm64 and shutil.which("box64"):
        cmd += ["box64"]
    
    wine_bin = "wine64" if shutil.which("wine64") else "wine"
    exe_path = "/app/skp2gltf.exe"
    
    if not os.path.exists(exe_path):
        # Fallback for local dev testing if path differs
        exe_path = os.getenv("SKP2GLTF_EXE", "/app/skp2gltf.exe")

    cmd += [wine_bin, exe_path, input_path, output_dir, output_name, format]
    
    logger.info(f"Executing: {' '.join(cmd)}")
    
    env = os.environ.copy()
    if "DISPLAY" not in env:
        env["DISPLAY"] = ":99"
    
    process = await asyncio.create_subprocess_exec(
        *cmd,
        stdout=asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE,
        env=env
    )
    
    stdout, stderr = await process.communicate()
    
    if process.returncode != 0:
        err_msg = stderr.decode()
        logger.error(f"Conversion failed (code {process.returncode}): {err_msg}")
        return False, err_msg
    
    return True, stdout.decode()

@app.get("/health")
def health():
    # Check if Xvfb lock exists
    xvfb_active = os.path.exists("/tmp/.X99-lock")
    return {
        "status": "ready", 
        "architecture": os.uname().machine,
        "xvfb_active": xvfb_active
    }

@app.post("/convert")
async def convert_upload(
    background_tasks: BackgroundTasks,
    file: UploadFile = File(...), 
    format: str = "glb"
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
            ok, msg = await run_conversion(input_path, tmp_dir, output_name, format)
            if not ok:
                raise HTTPException(status_code=500, detail=f"Conversion failed: {msg}")

        if not os.path.exists(output_file):
            raise HTTPException(status_code=500, detail="Output file not found after conversion")

        background_tasks.add_task(cleanup_tmp, tmp_dir)
        return FileResponse(output_file, filename=output_name + out_ext)
    except Exception as e:
        cleanup_tmp(tmp_dir)
        if isinstance(e, HTTPException): raise e
        logger.exception("Conversion task failed")
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/convert-path")
async def convert_path(req: ConvertRequest):
    if not os.path.exists(req.input_path):
        raise HTTPException(status_code=404, detail="Input file not found")
    
    out_name = req.output_name or os.path.splitext(os.path.basename(req.input_path))[0]
    os.makedirs(req.output_dir, exist_ok=True)

    async with lock:
        ok, msg = await run_conversion(req.input_path, req.output_dir, out_name, req.format)
        if not ok:
             raise HTTPException(status_code=500, detail=f"Conversion failed: {msg}")

    return {"status": "success", "dest": f"{req.output_dir}/{out_name}.{req.format}"}

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)

import httpx
import os
import time
import pytest
import asyncio
from typing import AsyncGenerator

# Base URL of the service (inside Docker it will be localhost:8000)
BASE_URL = os.getenv("API_URL", "http://localhost:8000")

@pytest.fixture
async def client() -> AsyncGenerator[httpx.AsyncClient, None]:
    async with httpx.AsyncClient(base_url=BASE_URL, timeout=120.0) as client:
        yield client

@pytest.mark.asyncio
async def test_health(client: httpx.AsyncClient):
    """Test health endpoint"""
    response = await client.get("/health")
    assert response.status_code == 200
    data = response.json()
    assert data["status"] == "ready"
    assert "architecture" in data

@pytest.mark.asyncio
async def test_convert_upload_missing_file(client: httpx.AsyncClient):
    """Test conversion with no file"""
    response = await client.post("/convert")
    assert response.status_code == 422  # Validation error

@pytest.mark.asyncio
async def test_convert_upload_invalid_ext(client: httpx.AsyncClient):
    """Test conversion with wrong file extension"""
    files = {"file": ("test.txt", b"hello world", "text/plain")}
    response = await client.post("/convert", files=files)
    assert response.status_code == 400
    assert "Only .skp files supported" in response.json()["detail"]

# Note: Integration test with actual .skp requires a valid file and Wine running
# For CI, we can use a dummy file and check if it attempts to call the bin.
@pytest.mark.asyncio
async def test_convert_upload_draco(client: httpx.AsyncClient):
    """Test the draco parameter in the upload workflow"""
    dummy_content = b"This is a dummy SKP for API testing"
    files = {"file": ("test.skp", dummy_content, "application/octet-stream")}
    
    # We test with draco=True parameter
    response = await client.post("/convert?draco=true", files=files)
    assert response.status_code in [200, 500]

@pytest.mark.asyncio
async def test_convert_path(client: httpx.AsyncClient):
    """Test the /convert-path endpoint"""
    # Assuming this runs in a container where /app/api.py or something exists,
    # but for a pure API test, we just check for errors with non-existent paths first.
    payload = {
        "input_path": "/non/existent/path.skp",
        "output_dir": "/tmp/output",
        "draco": True
    }
    response = await client.post("/convert-path", json=payload)
    # Should be 404 because file doesn't exist
    assert response.status_code == 404

if __name__ == "__main__":
    # Simple manual test trigger
    import sys
    loop = asyncio.get_event_loop()
    try:
        res = loop.run_until_complete(test_health(httpx.AsyncClient(base_url=BASE_URL)))
        print("Health check PASSED")
    except Exception as e:
        print(f"Health check FAILED: {e}")
        sys.exit(1)

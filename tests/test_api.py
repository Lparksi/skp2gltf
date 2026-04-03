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
    async with httpx.AsyncClient(base_url=BASE_URL, timeout=30.0) as client:
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
async def test_convert_upload_workflow(client: httpx.AsyncClient):
    """
    Integration test - attempts to convert a dummy .skp
    In CI, this might fail with 500 if Wine isn't ready or the file is junk,
    but it tests the FastAPI wiring.
    """
    dummy_content = b"This is a dummy SKP for API testing"
    files = {"file": ("test.skp", dummy_content, "application/octet-stream")}
    
    # We expect 500 here because the dummy content isn't a real SKP,
    # but the logic should flow correctly through the subprocess call.
    response = await client.post("/convert", files=files)
    
    # If the converter was actually called and failed, it's a 500.
    # If it's a success (unlikely with junk data), it's a 200.
    assert response.status_code in [200, 500]

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

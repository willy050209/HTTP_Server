from fastapi import FastAPI
from fastapi.responses import PlainTextResponse, JSONResponse
import uvicorn

app = FastAPI(docs_url=None, redoc_url=None, openapi_url=None)

@app.get("/plaintext", response_class=PlainTextResponse)
def get_plaintext():
    return "Hello, World!"

@app.get("/json")
def get_json():
    return {"message": "Hello, World!", "timestamp": 1700000000}

@app.get("/users/{id}")
def get_user(id: int):
    return {"id": id, "name": f"User_{id}", "status": "active"}

if __name__ == "__main__":
    uvicorn.run(app, host="127.0.0.1", port=8000, log_level="warning", access_log=False)

FROM python:3.12-slim

WORKDIR /app

# 先装依赖, 利用层缓存
COPY pyproject.toml ./
RUN pip install --no-cache-dir \
    "fastapi>=0.110" "uvicorn[standard]>=0.29" "aiomqtt>=2.0" \
    "asyncpg>=0.29" "pydantic>=2.6" "pydantic-settings>=2.2" \
    "python-dotenv>=1.0" "httpx>=0.27"

COPY app ./app

EXPOSE 8000
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]

FROM python:slim-buster
WORKDIR /app
COPY requirements.txt .
RUN pip3 install -r requirements.txt

COPY templates ./templates
COPY app.py .


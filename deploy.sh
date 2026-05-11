#!/bin/bash
SERVICE=$1

docker compose build --no-cache "$SERVICE"
docker compose up -d
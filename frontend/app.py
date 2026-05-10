import json
import requests
from flask import Flask, render_template, request
import sqlite3

app = Flask(__name__)

# The Flask route, defining the main behaviour of the webserver:
@app.route("/")
def home():
    return render_template('home.html')

@app.route("/api/<path:path>")
def proxy(path):
    return requests.get(f"http://backend:5000/api/{path}").content
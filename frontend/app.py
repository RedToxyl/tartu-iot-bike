import json

from flask import Flask, render_template, request
import sqlite3

app = Flask(__name__)
backend_url = "http://172.17.67.151:5000"  # URL to the backend service

# The Flask route, defining the main behaviour of the webserver:
@app.route("/")
def home():
    return render_template('home.html', backend_url=backend_url)

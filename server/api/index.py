# api/index.py
from flask import Flask, request, jsonify
import json, os, psycopg2
from flask_cors import CORS

app = Flask(__name__, static_folder='../public', static_url_path='')
CORS(app)
DATABASE_URL = os.environ["DATABASE_URL"]

def get_conn():
    return psycopg2.connect(DATABASE_URL)

@app.route('/')
def index():    
    return app.send_static_file('index.html')

@app.route('/test')
def test():    
    return jsonify({"status": "Flask is working"})

@app.route("/api/data", methods=["POST"])
def post_data():
    data = request.get_json()
    conn = get_conn()
    cur = conn.cursor()
    cur.execute("INSERT INTO entries (data) VALUES (%s) RETURNING id, created_at", [json.dumps(data)])
    row_id, created_at = cur.fetchone()
    conn.commit()
    cur.close(); conn.close()
    return jsonify({"status": "ok", "id": row_id, "created_at": str(created_at)})

@app.route("/api/data", methods=["GET"])
def get_data():
    conn = get_conn()
    cur = conn.cursor()
    cur.execute("SELECT id, data, created_at FROM entries ORDER BY created_at DESC")
    rows = cur.fetchall()
    cur.close(); conn.close()
    entries = [{"id": r[0], "data": r[1], "created_at": str(r[2])} for r in rows]
    return jsonify({"entries": entries})

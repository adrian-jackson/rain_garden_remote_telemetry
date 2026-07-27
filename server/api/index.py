# api/index.py
from flask import Flask, request, jsonify, send_from_directory
import json, os, psycopg2
from flask_cors import CORS
from dotenv import load_dotenv

try:
    from secrets import API_KEY
    from secrets import DATABASE_URL
except ModuleNotFoundError:
    load_dotenv()  # Load environment variables from .env file
    API_KEY = os.getenv('API_KEY')
    DATABASE_URL = os.getenv('DATABASE_URL')

app = Flask(__name__, static_folder='.', static_url_path='')
CORS(app)


def get_conn():
    return psycopg2.connect(DATABASE_URL)

@app.route('/')
def index():    
    return send_from_directory('.', 'index.html')


@app.route('/test')
def test():    
    return jsonify({"status": "Flask is working"})

@app.route("/api/data", methods=["POST"])
def post_data():
    provided_key = request.headers.get('X-API-Key')
    if not provided_key or provided_key != API_KEY:        
        return jsonify({'error': 'Unauthorized'}), 401
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
    # GET: Retrieve last N records
    limit = request.args.get('limit', 50, type=int)
    
    try:
        conn = psycopg2.connect(os.getenv('DATABASE_URL'))
        cur = conn.cursor()
        cur.execute('''
            SELECT site_id, temp_f, humidity, precipitation, inflow, outflow, downflow, timestamp
            FROM sensor_data
            ORDER BY timestamp DESC
            LIMIT %s
        ''', (limit,))
        
        columns = [desc[0] for desc in cur.description]
        records = [dict(zip(columns, row)) for row in cur.fetchall()]
        
        cur.close()
        conn.close()
        
        return jsonify(records), 200
    
    except Exception as e:
        print(f"Error: {e}")  # Show in Vercel logs
        return jsonify({'error': str(e)}), 500


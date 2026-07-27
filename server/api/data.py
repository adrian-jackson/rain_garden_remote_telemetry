from http.server import BaseHTTPRequestHandler
import json
import os
import psycopg2

DATABASE_URL = os.environ["DATABASE_URL"]

def get_conn():
    return psycopg2.connect(DATABASE_URL)

class handler(BaseHTTPRequestHandler):
    def _send_json(self, status, payload):
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(json.dumps(payload, default=str).encode())

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length)
        try:
            data = json.loads(body)
        except json.JSONDecodeError:
            self._send_json(400, {"error": "invalid JSON"})
            return

        conn = get_conn()
        cur = conn.cursor()
        cur.execute(
            "INSERT INTO entries (data) VALUES (%s) RETURNING id, created_at",
            [json.dumps(data)]
        )
        row_id, created_at = cur.fetchone()
        conn.commit()
        cur.close()
        conn.close()

        self._send_json(200, {"status": "ok", "id": row_id, "created_at": created_at})

    def do_GET(self):
        conn = get_conn()
        cur = conn.cursor()
        cur.execute("SELECT id, data, created_at FROM entries ORDER BY created_at DESC")
        rows = cur.fetchall()
        cur.close()
        conn.close()

        entries = [{"id": r[0], "data": r[1], "created_at": r[2]} for r in rows]
        self._send_json(200, {"entries": entries})

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()
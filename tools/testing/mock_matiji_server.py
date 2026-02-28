#!/usr/bin/env python3
"""Minimal mock server for crawler integration tests."""
from http.server import BaseHTTPRequestHandler, HTTPServer

HOST, PORT = "127.0.0.1", 18080

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path.startswith('/exam/ojquestion/123'):
            self.send_response(200)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.end_headers()
            self.wfile.write(b"<title>A+B - \xe9\xa9\xac\xe8\xb9\x84\xe9\x9b\x86</title><div>\xe8\xbe\x93\xe5\x85\xa5\xe6\xa0\xb7\xe4\xbe\x8b</div><pre>1 2</pre><div>\xe8\xbe\x93\xe5\x87\xba\xe6\xa0\xb7\xe4\xbe\x8b</div><pre>3</pre>")
        elif self.path.startswith('/blocked'):
            self.send_response(403)
            self.end_headers()
        elif self.path.startswith('/timeout'):
            return
        else:
            self.send_response(404)
            self.end_headers()

    def log_message(self, format, *args):
        return

if __name__ == '__main__':
    HTTPServer((HOST, PORT), Handler).serve_forever()

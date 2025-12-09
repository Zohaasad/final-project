#!/usr/bin/env python3
import socket
import threading
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import webbrowser
import os
BACKEND_HOST = '127.0.0.1'
# 16.16.57.198
BACKEND_PORT = 8080
WEB_PORT = 3000

# Session storage: clientId -> (socket, userId, username)
sessions = {}
session_counter = 0
session_lock = threading.Lock()

def get_or_create_session():
    """Get or create a persistent connection to backend"""
    global session_counter
    
    with session_lock:
        # For simplicity, use a single shared session
        session_id = 'main_session'
        
        if session_id not in sessions or not sessions[session_id].get('connected'):
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.connect((BACKEND_HOST, BACKEND_PORT))
                sessions[session_id] = {
                    'socket': sock,
                    'connected': True,
                    'userId': -1,
                    'username': ''
                }
                print(f"[PROXY] Created new session connection")
            except Exception as e:
                print(f"[PROXY] Failed to connect: {e}")
                return None
        
        return sessions[session_id]

def send_to_backend(message):
    """Send message to C++ server using persistent connection"""
    try:
        session = get_or_create_session()
        if not session:
            return "ERROR|Cannot connect to server"
        
        sock = session['socket']
        
        # Send message
        sock.sendall(message.encode())
        
        # Receive response
        response = sock.recv(8192).decode()
        
        return response
    except Exception as e:
        print(f"[PROXY] Backend Error: {e}")
        # Mark session as disconnected
        with session_lock:
            if 'main_session' in sessions:
                sessions['main_session']['connected'] = False
        return f"ERROR|Connection error: {str(e)}"

class ProxyHandler(BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass  # Suppress logs
    
    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.end_headers()
    
    def do_GET(self):
        parsed = urlparse(self.path)
        
        # Serve HTML file
        if parsed.path == '/' or parsed.path == '/index.html':
            self.send_response(200)
            self.send_header('Content-type', 'text/html')
            self.end_headers()
            
            html_file = os.path.join(os.path.dirname(__file__), 'index.html')
            try:
                with open(html_file, 'r') as f:
                    self.wfile.write(f.read().encode())
            except FileNotFoundError:
                self.wfile.write(b"<h1>Error: index.html not found!</h1>")
            return
        
        # Handle API calls
        if parsed.path.startswith('/api'):
            params = parse_qs(parsed.query)
            action = params.get('action', [''])[0]
            
            # Build message according to Protocol.hpp format
            message = ""
            
            if action == 'login':
                username = params.get('username', [''])[0]
                password = params.get('password', [''])[0]
                message = f"LOGIN|{username}|{password}"
                print(f"[PROXY] Sending: {message}")
            
            elif action == 'register':
                username = params.get('username', [''])[0]
                password = params.get('password', [''])[0]
                message = f"REGISTER|{username}|{password}"
                print(f"[PROXY] Sending: {message}")
            elif action == 'search_file':
                name = params.get('name', [''])[0]
                message = f"SEARCH_FILE|{name}"
                print(f"[PROXY] Sending: {message}")
            elif action == 'create_file':
                name = params.get('name', [''])[0]
                content = params.get('content', [''])[0]
                expiry = params.get('expiry', ['3600'])[0]
                message = f"CREATE_FILE|{name}|{content}|{expiry}"
                print(f"[PROXY] Sending: CREATE_FILE|{name}|...|{expiry}")
            
            elif action == 'write_file':
                name = params.get('name', [''])[0]
                content = params.get('content', [''])[0]
                message = f"WRITE_FILE|{name}|{content}"
                print(f"[PROXY] Sending: WRITE_FILE|{name}|...")
            
            elif action == 'read_file':
                name = params.get('name', [''])[0]
                message = f"READ_FILE|{name}"
                print(f"[PROXY] Sending: {message}")
            
            elif action == 'list_files':
                message = "LIST_FILES"
                print(f"[PROXY] Sending: {message}")
            
            elif action == 'move_to_bin':
                name = params.get('name', [''])[0]
                message = f"MOVE_TO_BIN|{name}"
                print(f"[PROXY] Sending: {message}")
            
            elif action == 'retrieve':
                name = params.get('name', [''])[0]
                message = f"RETRIEVE_FROM_BIN|{name}"
                print(f"[PROXY] Sending: {message}")
            
            elif action == 'change_expiry':
                name = params.get('name', [''])[0]
                expiry = params.get('expiry', ['3600'])[0]
                message = f"CHANGE_EXPIRY|{name}|{expiry}"
                print(f"[PROXY] Sending: {message}")
            
            elif action == 'delete':
                name = params.get('name', [''])[0]
                message = f"DELETE_PERMANENTLY|{name}"
                print(f"[PROXY] Sending: {message}")
            
            elif action == 'truncate':
                name = params.get('name', [''])[0]
                message = f"TRUNCATE_FILE|{name}"
                print(f"[PROXY] Sending: {message}")
            
            elif action == 'disk_stats':
                message = "DISK_STATS"
                print(f"[PROXY] Sending: {message}")
            
            elif action == 'logout':
                message = "LOGOUT"
                print(f"[PROXY] Sending: {message}")
            
            else:
                message = "UNKNOWN"
            
            # Send to backend and get response
            response = send_to_backend(message)
            print(f"[PROXY] Received: {response[:100]}...")
            
            # Send response back to browser
            self.send_response(200)
            self.send_header('Content-type', 'text/plain')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(response.encode())

def run_server():
    server = HTTPServer(('localhost', WEB_PORT), ProxyHandler)
    print(f"\n{'='*50}")
    print(f"🚀 File Management System - Web UI")
    print(f"{'='*50}")
    print(f"✓ Proxy server running on: http://localhost:{WEB_PORT}")
    print(f"✓ Backend server: {BACKEND_HOST}:{BACKEND_PORT}")
    print(f"✓ Opening browser...")
    print(f"{'='*50}\n")
    
    # Open browser after a short delay
    threading.Timer(1.5, lambda: webbrowser.open(f'http://localhost:{WEB_PORT}')).start()
    
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n\n✓ Server stopped")
        server.shutdown()

if __name__ == '__main__':
    if not os.path.exists('index.html'):
        print("❌ ERROR: index.html not found in current directory!")
        print("Please make sure index.html is in the same folder as proxy.py")
        exit(1)
    
    print("\n⚠️  Make sure your C++ server is running on port 8080!")
    print("Starting web server...\n")
    run_server()

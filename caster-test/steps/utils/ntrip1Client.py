import select
import socket
import base64
import time

class Ntrip1Client:
    def __init__(self, mount, user, password, eol):
        self.mount = mount
        self.user = user
        self.password = password
        self.eol = eol
        self.socket = self.initialize_socket()

    def stop(self):
        try:
            self.socket.close()
        except Exception as e:
            print(e)

    def initialize_socket(self):
        request = (
            f"GET /{self.mount} HTTP/1.0\n"
            f"User-Agent: NTRIP LOADTEST/0.0.0\n"
            f"Authorization: Basic {base64.b64encode((self.user + ':' + self.password).encode()).decode()}\n"
            "\n"
        ).replace("\n", self.eol)

        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(("localhost", 2101))
            sock.setblocking(False)
            os = sock.makefile('wb')
            os.write(request.encode())
            os.flush()

            in_stream = sock.makefile('rb')
            while True:
                ready = select.select([sock], [], [], 0.5)  # Wait for the socket to be ready for reading
                if ready[0]:
                    while True:
                        s = in_stream.readline().strip()
                        if not s:
                            break
                        print(s.decode())
                    return sock
                else:
                    print("Client not ready after 0.5s. Exiting")
                    return None
        except Exception as e:
            print(e)
            return None

    def get_line_received(self):
        try:
            in_stream = self.socket.makefile('rb')
            line_received = []
            while True:
                ready = select.select([self.socket], [], [], 0.5)  # Wait for the socket to be ready for reading
                if ready[0]:
                    while True:
                        line = in_stream.readline().strip()
                        if not line:
                            break
                        line_received.append(line.decode())
                else:
                    break
            return line_received
        except Exception as e:
            print(e)
            return []

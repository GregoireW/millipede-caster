import socket

from steps.utils.BaseNtripEmitter import BaseNtripEmitter


class BaseNtrip1Emitter(BaseNtripEmitter):
    def __init__(self, base, password, eol):
        self.base = base
        self.password = password
        self.eol = eol
        self.socket = self.initialize_socket()
        try:
            self.send_message()
        except Exception as e:
            print(e)

    def stop(self):
        try:
            self.socket.close()
        except Exception as e:
            print(e)

    def initialize_socket(self):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(("localhost", 2101))
            request = (
                f"SOURCE {self.password} {self.base}\n"
                f"Source-Agent: NTRIP LOADTEST/0.0.0\n"
                f"STR:\n"
                "\n"
            ).replace("\n", self.eol)
            sock.sendall(request.encode())
            in_stream = sock.makefile('r')
            s = ""
            while (s := in_stream.readline().strip()):
                print(s)
            print(f"Source {self.base} ok")
            return sock
        except Exception as e:
            print(e)
            return None

    def send_message(self):
        if self.socket:
            os = self.socket.makefile('w')
            os.write(f"base ~{self.base}~\n")
            os.flush()
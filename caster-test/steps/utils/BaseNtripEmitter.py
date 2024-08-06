class BaseNtripEmitter:
    def send_message(self):
        raise NotImplementedError
    def stop(self):
        raise NotImplementedError

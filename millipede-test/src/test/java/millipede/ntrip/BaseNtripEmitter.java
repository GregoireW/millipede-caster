package millipede.ntrip;

import java.io.IOException;

public interface BaseNtripEmitter {
    void stop();
    void sendMessage() throws IOException;
}

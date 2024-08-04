package millipede.ntrip;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.Socket;

public class BaseNtrip1Emitter implements BaseNtripEmitter {
    private final String eol;
    private final String base;
    private final String password;
    private final Socket socket;

    public BaseNtrip1Emitter(String base, String password, String eol){
        this.base=base;
        this.password=password;
        this.eol=eol;

        socket=initializeSocket();
        try {
            sendMessage();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    public void stop(){
        try {
            socket.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private Socket initializeSocket(){
        try {
            Socket sock = new Socket("localhost", 2101);
            String request = """
                    SOURCE $passwd $base
                    Source-Agent: NTRIP LOADTEST/0.0.0
                    STR:
                    
                    """.replace("$base", base)
                    .replace("$passwd", password)
                    .replace("\n", eol);

            var os = sock.getOutputStream();
            os.write(request.getBytes());
            var in = new BufferedReader(new InputStreamReader(sock.getInputStream()));
            String s = "";
            while (!(s = in.readLine()).isEmpty()) {
                System.out.println(s);
            }
            System.out.println("Source " + base + " ok");
            return sock;
        }catch(Exception e){
            e.printStackTrace();
            return null;
        }
    }

    @Override
    public void sendMessage() throws IOException {
        if (socket!=null){
            var os=socket.getOutputStream();
            os.write(("base ~" + base + "~\n").getBytes());
            os.flush();
        }
    }
}

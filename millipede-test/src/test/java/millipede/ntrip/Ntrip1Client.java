package millipede.ntrip;

import java.io.BufferedReader;
import java.io.IOException;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;

public class Ntrip1Client {
    private final String mount;
    private final String user;
    private final String password;
    private final String eol;

    private final Socket socket;

    public Ntrip1Client(String mount, String login, String password, String eol) {
        this.mount = mount;
        this.user = login;
        this.password = password;
        this.eol = eol;

        socket=initializeSocket();
    }

    public void stop(){
        try {
            socket.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private Socket initializeSocket() {
        String request = """
                GET /$mount HTTP/1.0
                User-Agent: NTRIP LOADTEST/0.0.0
                Authorization: Basic $basic
                
                """.replace("$mount", mount)
                .replace("$basic", java.util.Base64.getEncoder().encodeToString((user + ":" + password).getBytes()))
                .replace("\n",eol);

        try {
            var sock = new Socket("localhost", 2101);
            var os = sock.getOutputStream();
            var in = sock.getInputStream();

            os.write(request.getBytes());

            BufferedReader br = new BufferedReader(new java.io.InputStreamReader(in));
            String s;
            while (!(s=br.readLine()).isEmpty()) {
                System.out.println(s);
            }
            return sock;
        }catch (IOException e) {
            e.printStackTrace();
        }
        return null;
    }

    public List<String> getLineReceived() throws IOException{
        // Wait 50ms is mandatory because of the default 40ms TCP ACK delay in all the OS (congestion control)
        try {
            Thread.sleep(50);
        }catch(InterruptedException e){
            Thread.currentThread().interrupt();
        }
        var in = socket.getInputStream();
        BufferedReader br = new BufferedReader(new java.io.InputStreamReader(in));

        var lineReceived=new ArrayList<String>();
        while (br.ready()) {
            lineReceived.add(br.readLine());
        }
        return lineReceived;
    }
}

package millipede;

import io.cucumber.core.exception.CucumberException;
import io.cucumber.java.After;
import io.cucumber.java.AfterAll;
import io.cucumber.java.BeforeAll;
import io.cucumber.java.en.Given;
import io.cucumber.java.en.Then;
import io.cucumber.java.en.When;
import millipede.ntrip.BaseNtrip1Emitter;
import millipede.ntrip.BaseNtripEmitter;
import millipede.ntrip.Ntrip1Client;

import java.io.IOException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;

import static org.junit.jupiter.api.Assertions.assertEquals;

public class StepDefinitions {
    private static Process casterProcess;

    private final List<BaseNtripEmitter> processes=new ArrayList<>();
    private Ntrip1Client client;

    @BeforeAll
    public static void startCaster() {
        try {
            Path.of("./target/log/access.log").getParent().toFile().mkdirs();
            String caster=System.getenv("CASTER_EXECUTABLE");
            caster=caster==null?"../cmake-build-coverage/caster":caster;
            casterProcess = new ProcessBuilder(caster, "-c", "./src/test/resources/config/caster.yaml")
                    .redirectOutput(ProcessBuilder.Redirect.INHERIT)
                    .redirectError(ProcessBuilder.Redirect.INHERIT)
                    .start();
        } catch (IOException e) {
            throw new CucumberException("Failed to start the caster", e);
        }
    }

    @AfterAll
    public static void stopCaster() {
        if (casterProcess != null) {
            try {
                long res = Runtime.getRuntime().exec(new String[]{"kill", "-SIGINT", Long.toString(casterProcess.pid())}).waitFor();
                if (res == 0)
                    casterProcess.waitFor();
            }catch(IOException e){
                // no operation, just ignore
            }catch(InterruptedException e){
                Thread.currentThread().interrupt();
            }
            // if the process is not killed, this will.
            casterProcess.destroy();
        }
    }

    @After
    public void stopBases() {
        processes.forEach(BaseNtripEmitter::stop);
        processes.clear();
        if (client!=null){
            client.stop();
            client=null;
        }
    }

    private void emitMessage() {
        for (var p : processes) {
            try {
                p.sendMessage();
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    //@Given("The base {string} is emitting")
    @Given("^The base (.*) is emitting(?: with end of line (.*))?$")
    public void baseEmit(String base, String eol) { // , String password, String eol
        System.out.println("EOL is "+eol);
        eol=eol==null?"":eol;
        String password=base+"pwd";
        processes.add(new BaseNtrip1Emitter(base, password, "LF".equals(eol)?"\n":"\r\n"));
    }

    @When("client connect to the base {string}") // ( with authentication {string} {string})?( with eol {string})?
    public void clientConnection(String mount) { //, String login, String password, String eol
        client = new Ntrip1Client(mount, "", "", "\r\n");
        emitMessage();
    }

    @When("client connect to the base {string} with end of line {string}")
    public void clientConnectToTheBaseWithEndOfLine(String mount, String eol) {
        client = new Ntrip1Client(mount, "", "", "LF".equals(eol)?"\n":"\r\n");
        emitMessage();
    }

    @Then("client should have received a message from base {string} in the last message")
    public void checkLastMessage(String expectedBase) throws IOException{
        var lst=client.getLineReceived();
        var lastLine=lst.getLast();
        assertEquals("base ~" + expectedBase + "~", lastLine);
    }
}

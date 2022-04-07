package at.rocworks;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.TooManyListenersException;
import java.util.logging.Level;
import java.util.logging.Logger;

/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/**
 *
 * @author vogler
 */
public class ArduinoIOC {    

    final char STX = 0x02;
    final char ETX = 0x03;
    final char ACK = 0x06;
    final char BEL = 0x07;    
    
    public static void main(String[] args) {
        (new ArduinoIOC()).run();
    }
    
    public void run() {

    }

    
    public void digitalConfig(OutputStream out, int pin, int mode, int rate, int oldnew, int active) throws IOException {
        System.out.println("send "+pin+"/"+mode+"/"+rate+"/"+oldnew+"/"+active);
        out.write((STX+String.format("1D%02d%06d", pin, mode)+ETX).getBytes()); out.flush();
        out.write((STX+String.format("2D%02d%06d", pin, rate)+ETX).getBytes()); out.flush();
        out.write((STX+String.format("4D%02d%01d", pin, oldnew)+ETX).getBytes()); out.flush();
        out.write((STX+String.format("3D%02d%01d", pin, active)+ETX).getBytes()); out.flush();        
    }
    

}

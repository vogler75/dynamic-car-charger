package at.rocworks;

import java.text.SimpleDateFormat;

import java.util.Date;

public class Debug {
    
    final static private SimpleDateFormat fmt = new SimpleDateFormat("HH:mm:ss.SSS");          
    
    public static void println(String s) {
        System.out.println(fmt.format(new Date()) + " " + s);  
    }
}

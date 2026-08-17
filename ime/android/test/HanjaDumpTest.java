import com.cuime.HanjaDict;import java.nio.file.*;import java.io.*;import java.util.*;
public class HanjaDumpTest{public static void main(String[] a)throws Exception{
 List<String> qs=Files.readAllLines(Paths.get("/tmp/queries.txt"),java.nio.charset.StandardCharsets.UTF_8);
 PrintWriter w=new PrintWriter(new OutputStreamWriter(new FileOutputStream("/tmp/hj_java.txt"),"UTF-8"));
 for(String q:qs){ if(q.isEmpty())continue; w.println(q+"\t"+String.join(" ",HanjaDict.lookup(q))); }
 w.close();}}

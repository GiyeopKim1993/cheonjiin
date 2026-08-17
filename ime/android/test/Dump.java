import com.cuime.core.CheonjiinCore;import java.util.*;import java.io.*;
public class Dump{public static void main(String[] a)throws Exception{
 PrintWriter w=new PrintWriter(new OutputStreamWriter(new FileOutputStream("/tmp/xl/java.txt"),"UTF-8"));
 for(char c:CheonjiinCore.CHO.toCharArray())for(char v:CheonjiinCore.JUNG.toCharArray())
 for(char j:CheonjiinCore.JONG.toCharArray()){
  String ch=CheonjiinCore.compose(""+c,""+v,(""+j).trim());
  w.println(ch+"\t"+String.join(" ",CheonjiinCore.encode(ch,false)));}
 w.close();}}

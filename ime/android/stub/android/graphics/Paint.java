package android.graphics;
public class Paint {
    public static final int ANTI_ALIAS_FLAG=1;
    public Paint(){} public Paint(int f){}
    public enum Align{LEFT,CENTER,RIGHT}
    public static class Style { public static final Style STROKE=new Style(), FILL=new Style(); }
    public void setColor(int c){} public void setStyle(Style s){}
    public void setStrokeWidth(float w){} public void setTextAlign(Align a){}
    public void setTextSize(float s){} public void setFakeBoldText(boolean b){}
    public float measureText(String s){ return s.length()*20f; }
}

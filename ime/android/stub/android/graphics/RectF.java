package android.graphics;
public class RectF {
    public float left,top,right,bottom;
    public RectF(float l,float t,float r,float b){left=l;top=t;right=r;bottom=b;}
    public float centerX(){return (left+right)/2;} public float centerY(){return (top+bottom)/2;}
}

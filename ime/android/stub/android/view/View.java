package android.view;
import android.content.Context;
import android.graphics.Canvas;
import android.content.res.Resources;
public class View {
    public View(Context c) { ctx = c; }
    protected Context ctx;
    public Resources getResources(){ return new Resources(); }
    public int getWidth(){ return 1080; }
    public int getHeight(){ return 800; }
    public void invalidate(){}
    protected void onDraw(Canvas c){}
    protected void onMeasure(int w,int h){}
    protected void setMeasuredDimension(int w,int h){}
    public boolean onTouchEvent(MotionEvent e){ return true; }
    public static class MeasureSpec {
        public static int getSize(int s){ return s & 0x00FFFFFF; }
        public static int getMode(int s){ return 0; }
    }
}

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
    /* 실제 View 에 있는 public 메서드. 스텁에 없으면 하위 클래스가
       private 로 잘못 오버라이드해도 통과해버린다(실제로 겪음). */
    public void cancelLongPress() { }
}

package android.inputmethodservice;
import android.content.Context;
import android.view.View;
import android.view.inputmethod.*;
public class InputMethodService extends Context {
    public View onCreateInputView(){ return null; }
    public void onStartInput(EditorInfo i,boolean r){}
    public void onFinishInput(){}
    public InputConnection getCurrentInputConnection(){ return null; }
    public EditorInfo getCurrentInputEditorInfo(){ return null; }
}

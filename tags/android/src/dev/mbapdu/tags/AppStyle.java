package dev.mbapdu.tags;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.graphics.drawable.StateListDrawable;
import android.graphics.drawable.ColorDrawable;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

/** Small native Android palette; no remote assets or UI dependencies. */
final class AppStyle {
    static final int BG=Color.BLACK,SURFACE=0xff121315,RAISED=0xff1e2024;
    static final int TEXT=0xfff5f5f6,MUTED=0xff959ba5,BLUE=0xffb7d5ff,BLUE_BG=0xff1d3049;
    static final int GREEN=0xffa9d5b3,RED=0xffffb4ab;
    static int dp(Context c,float n){return Math.round(n*c.getResources().getDisplayMetrics().density);}
    static GradientDrawable shape(int color,int radius,Context c){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(c,radius));return d;}
    static LinearLayout column(Context c){LinearLayout l=new LinearLayout(c);l.setOrientation(LinearLayout.VERTICAL);return l;}
    static LinearLayout row(Context c){LinearLayout l=new LinearLayout(c);l.setGravity(Gravity.CENTER_VERTICAL);return l;}
    static TextView text(Context c,String value,int size,int color){TextView t=new TextView(c);t.setText(value);t.setTextSize(size);t.setTextColor(color);t.setIncludeFontPadding(false);return t;}
    static void medium(TextView t){t.setTypeface(Typeface.create("sans-serif-medium",Typeface.NORMAL));}
    static LinearLayout.LayoutParams width(int width,int height){return new LinearLayout.LayoutParams(width,height);}
    static void gap(LinearLayout p,int height){View v=new View(p.getContext());p.addView(v,width(1,dp(p.getContext(),height)));}
    static Button button(Context c,String label,boolean primary,View.OnClickListener action){
        Button b=new Button(c);b.setText(label);b.setAllCaps(false);b.setTextSize(15);medium(b);
        int foreground=primary?0xff101419:TEXT;
        b.setTextColor(new ColorStateList(new int[][]{new int[]{-android.R.attr.state_enabled},new int[]{}},new int[]{primary?0xff101419:MUTED,foreground}));
        b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x338eadd1),shape(primary?BLUE:RAISED,16,c),null));
        b.setMinHeight(dp(c,52));b.setMinimumHeight(dp(c,52));b.setMinWidth(0);b.setMinimumWidth(0);
        b.setPadding(dp(c,14),dp(c,10),dp(c,14),dp(c,10));b.setOnClickListener(action);return b;
    }
    static Button addButton(LinearLayout p,String label,boolean primary,View.OnClickListener action){Button b=button(p.getContext(),label,primary,action);p.addView(b,width(-1,-2));return b;}
    static CheckBox checkBox(Context c){
        CheckBox box=new CheckBox(c);box.setMinWidth(0);box.setMinHeight(0);box.setMinimumWidth(0);box.setMinimumHeight(0);box.setPadding(0,0,0,0);
        StateListDrawable surface=new StateListDrawable();surface.addState(new int[]{android.R.attr.state_checked},shape(BLUE,12,c));surface.addState(new int[]{},shape(RAISED,12,c));
        box.setBackground(new RippleDrawable(ColorStateList.valueOf(0x338eadd1),surface,null));
        StateListDrawable mark=new StateListDrawable();
        int resource=c.getResources().getIdentifier("ic_sync_check","drawable",c.getPackageName());
        mark.addState(new int[]{android.R.attr.state_checked},c.getDrawable(resource));mark.addState(new int[]{},new ColorDrawable(Color.TRANSPARENT));
        box.setButtonTintList(null);box.setButtonDrawable(mark);return box;
    }
    static EditText input(Context c,String hint){EditText e=new EditText(c);e.setTextColor(TEXT);e.setHintTextColor(MUTED);e.setTextSize(16);e.setHint(hint);e.setBackground(shape(RAISED,14,c));e.setPadding(dp(c,16),dp(c,14),dp(c,16),dp(c,14));e.setBackgroundTintList(null);return e;}
    static void enable(View view,boolean enabled){if(view==null)return;view.setEnabled(enabled);view.setAlpha(enabled?1f:.6f);}
    static void window(Window w){if(w==null)return;w.setStatusBarColor(BG);w.setNavigationBarColor(BG);w.setNavigationBarContrastEnforced(false);w.setStatusBarContrastEnforced(false);WindowInsetsController c=w.getInsetsController();if(c!=null)c.setSystemBarsAppearance(0,WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS|WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS);}
    static void insets(LinearLayout root){insets(root,root);}
    static void insets(View target,LinearLayout root){int p=dp(root.getContext(),20);root.setPadding(p,p,p,p);target.setOnApplyWindowInsetsListener((v,i)->{android.graphics.Insets bars=i.getInsets(WindowInsets.Type.systemBars()|WindowInsets.Type.ime());root.setPadding(p+bars.left,p+bars.top,p+bars.right,p+bars.bottom);return WindowInsets.CONSUMED;});}
    static Dialog dialog(Activity activity,String title,LinearLayout body){
        Dialog dialog=new Dialog(activity,android.R.style.Theme_Material_NoActionBar);
        LinearLayout root=column(activity);root.setBackgroundColor(BG);insets(root);
        LinearLayout header=row(activity);TextView name=text(activity,title,25,TEXT);medium(name);header.addView(name,new LinearLayout.LayoutParams(0,-2,1));
        Button close=button(activity,"Закрыть",false,v->dialog.dismiss());close.setTextSize(13);header.addView(close,width(dp(activity,98),-2));root.addView(header);gap(root,24);
        root.addView(body,new LinearLayout.LayoutParams(-1,0,1));dialog.setContentView(root);dialog.setOnShowListener(d->{window(dialog.getWindow());dialog.getWindow().setLayout(-1,-1);root.requestApplyInsets();});return dialog;
    }
}

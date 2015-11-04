package ctrip.android.demo2;

import android.app.Activity;
import android.os.Bundle;
import android.widget.ImageView;
import android.widget.TextView;

public class MainActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.demo2_activity_main);
        TextView textView=(TextView)findViewById(R.id.demo2_textView3);
        textView.setText(R.string.sample_text);
        ImageView imageView=(ImageView)findViewById(R.id.demo2_imageView2);
        imageView.setImageResource(R.drawable.sample);
    }


}

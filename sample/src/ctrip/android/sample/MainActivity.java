package ctrip.android.sample;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;

public class MainActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        Button demo1 = (Button) findViewById(R.id.button);
        demo1.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                try {
                    startActivity(new Intent(getApplicationContext(), Class.forName("ctrip.android.demo1.MainActivity")));
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        });
        Button demo2 = (Button) findViewById(R.id.button2);
        demo2.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                try {
                    startActivity(new Intent(getApplicationContext(), Class.forName("ctrip.android.demo2.MainActivity")));
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        });
    }

}

package dev.dettmer.swipetype.sample;

import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import java.util.List;

/**
 * Demo launcher activity for the SwipeType sample IME.
 *
 * <p>The activity guides the user through two steps:
 * <ol>
 *   <li>Enable the "SwipeType Sample IME" keyboard in Android Settings →
 *       System → Languages &amp; input → Virtual keyboard.</li>
 *   <li>Tap the text field and switch to the SwipeType keyboard via the
 *       globe/keyboard icon in the navigation bar.</li>
 * </ol>
 *
 * <p>Once active, draw a swipe gesture across the on-screen keys to see
 * gesture-recognition candidates committed to the text field.
 */
public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Button btnEnableIme = findViewById(R.id.btn_enable_ime);
        Button btnPickIme   = findViewById(R.id.btn_pick_ime);
        EditText editText   = findViewById(R.id.edit_text_demo);
        TextView tvStatus   = findViewById(R.id.tv_status);

        // ── Step 1: open Android IME settings so the user can enable the keyboard
        btnEnableIme.setOnClickListener(v ->
                startActivity(new Intent(Settings.ACTION_INPUT_METHOD_SETTINGS)));

        // ── Step 2: show the IME picker so the user can switch to SwipeType
        btnPickIme.setOnClickListener(v -> {
            InputMethodManager imm = getSystemService(InputMethodManager.class);
            if (imm != null) imm.showInputMethodPicker();
        });

        updateStatus(tvStatus);
    }

    @Override
    protected void onResume() {
        super.onResume();
        TextView tvStatus = findViewById(R.id.tv_status);
        updateStatus(tvStatus);
    }

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    private void updateStatus(TextView tv) {
        if (tv == null) return;
        boolean enabled = isSwipeTypeEnabled();
        if (enabled) {
            tv.setText("✓ SwipeType IME is enabled. Tap the text field and switch to it.");
        } else {
            tv.setText("SwipeType IME is NOT enabled yet. Tap \"Enable IME\" above.");
        }
    }

    /** Returns true if our IME is listed as an enabled input method. */
    private boolean isSwipeTypeEnabled() {
        InputMethodManager imm = getSystemService(InputMethodManager.class);
        if (imm == null) return false;
        List<?> list = imm.getEnabledInputMethodList();
        String pkg = getPackageName();
        for (Object info : list) {
            // InputMethodInfo.getPackageName()
            try {
                String p = (String) info.getClass().getMethod("getPackageName").invoke(info);
                if (pkg.equals(p)) return true;
            } catch (Exception ignored) {}
        }
        return false;
    }
}

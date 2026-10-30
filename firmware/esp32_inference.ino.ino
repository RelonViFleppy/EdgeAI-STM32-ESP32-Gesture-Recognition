#include <Arduino.h>
#include <math.h>

#define RX_PIN 18
#define TX_PIN 17

HardwareSerial STM32_Serial(1);

const int WINDOW_SIZE = 50;  // 1 saniyelik hareket penceresi
const int STEP_SIZE   = 10;  // Her 200 ms'de bir cikarim yap (gecikmeyi onler)

int16_t buf_x[WINDOW_SIZE];
int16_t buf_y[WINDOW_SIZE];
int16_t buf_z[WINDOW_SIZE];
int buffer_count = 0;
int step_counter = 0;

// --- MODEL AGIRLIKLARI ---
const float SCALER_MEAN[15] = {115.661451f, 1327.388734f, 15886.782191f, 6154.425249f, 2414.121724f, 4824.327476f, -10651.145455f, -3057.236364f, 8328.090909f, 10780.054545f, 6955.945455f, 24199.963636f, 21431.200000f, 10013.181818f, 15871.872727f};
const float SCALER_SCALE[15] = {1829.484622f, 2605.728388f, 671.869824f, 5924.708087f, 1761.392945f, 4561.851490f, 10715.311762f, 1543.199816f, 6681.597303f, 8188.489710f, 6705.506636f, 6522.023116f, 18636.747845f, 7014.373096f, 13108.452194f};

const float W0[15][8] = {
    {-0.497251f, 0.569370f, 0.336619f, 0.231491f, -0.220727f, -0.435742f, -0.730309f, 0.282711f},
    {0.348954f, 0.347718f, -0.626153f, 0.628862f, 0.526258f, -0.487936f, -0.396777f, -0.199429f},
    {-0.036701f, -0.072122f, 0.067625f, -0.348681f, -0.056455f, -0.157761f, -0.021925f, -0.274858f},
    {0.336017f, 0.174428f, -0.434852f, -0.141670f, -0.085073f, -0.609187f, 0.394468f, -0.225806f},
    {-0.394003f, 0.515169f, 0.344588f, 0.448495f, -0.024794f, -0.604968f, 0.076320f, 0.053882f},
    {-0.629997f, 0.112863f, -0.612581f, 0.572414f, -0.064939f, -0.043680f, -0.390208f, 0.130496f},
    {-0.323924f, -0.206111f, 0.605420f, 0.431784f, 0.613097f, 0.555652f, -0.193086f, 0.322708f},
    {-0.166724f, -0.140656f, -0.552976f, -0.023906f, -0.027393f, -0.134087f, 0.143334f, -0.218861f},
    {-0.053602f, -0.071772f, -0.235193f, 0.155322f, -0.614744f, 0.703023f, 0.459110f, -0.413126f},
    {-0.115383f, 0.202676f, 0.082514f, 0.071370f, 0.081300f, -0.584071f, 0.143824f, -0.280271f},
    {0.389748f, 0.226895f, -0.301251f, -0.304943f, -0.014186f, -0.368093f, 0.101685f, 0.252925f},
    {0.237785f, 0.092067f, -0.522989f, 0.373650f, 0.448585f, -0.146099f, 0.090773f, 0.102345f},
    {0.402580f, -0.191467f, -0.611875f, -0.557139f, -0.656648f, -0.011498f, 0.101387f, 0.118761f},
    {0.393526f, -0.187344f, -0.216395f, 0.399188f, -0.099557f, -0.618934f, -0.330103f, -0.240478f},
    {0.273766f, 0.432835f, 0.003321f, 0.534082f, 0.491382f, -0.527296f, 0.217322f, 0.147518f}
};
const float BIAS0[8] = {0.710734f, 0.315392f, -0.053860f, -0.242512f, -0.095698f, 0.139743f, 0.633110f, 0.255527f};

const float W1[8][3] = {
    {-1.050408f, 0.184526f, -0.182262f},
    {-0.583988f, -0.471339f, -0.261499f},
    {0.812940f, -0.416102f, -0.159047f},
    {0.012058f, -0.356267f, 0.886643f},
    {0.330004f, -0.580357f, 0.254689f},
    {-0.156488f, -0.445568f, -0.872688f},
    {-0.223039f, 0.350825f, -0.997753f},
    {-0.270444f, 0.505648f, -0.216969f}
};
const float BIAS1[3] = {-0.747147f, 0.247237f, 0.498373f};

void run_neural_network() {
  float feats[15];
  float sum_x = 0, sum_y = 0, sum_z = 0;
  float min_x = 32767, max_x = -32768;
  float min_y = 32767, max_y = -32768;
  float min_z = 32767, max_z = -32768;

  for (int i = 0; i < WINDOW_SIZE; i++) {
    sum_x += buf_x[i]; sum_y += buf_y[i]; sum_z += buf_z[i];
    if (buf_x[i] < min_x) min_x = buf_x[i]; if (buf_x[i] > max_x) max_x = buf_x[i];
    if (buf_y[i] < min_y) min_y = buf_y[i]; if (buf_y[i] > max_y) max_y = buf_y[i];
    if (buf_z[i] < min_z) min_z = buf_z[i]; if (buf_z[i] > max_z) max_z = buf_z[i];
  }

  feats[0] = sum_x / WINDOW_SIZE;
  feats[1] = sum_y / WINDOW_SIZE;
  feats[2] = sum_z / WINDOW_SIZE;

  float sq_x = 0, sq_y = 0, sq_z = 0;
  for (int i = 0; i < WINDOW_SIZE; i++) {
    sq_x += (buf_x[i] - feats[0]) * (buf_x[i] - feats[0]);
    sq_y += (buf_y[i] - feats[1]) * (buf_y[i] - feats[1]);
    sq_z += (buf_z[i] - feats[2]) * (buf_z[i] - feats[2]);
  }
  feats[3] = sqrt(sq_x / WINDOW_SIZE);
  feats[4] = sqrt(sq_y / WINDOW_SIZE);
  feats[5] = sqrt(sq_z / WINDOW_SIZE);

  feats[6] = min_x; feats[7] = min_y; feats[8] = min_z;
  feats[9] = max_x; feats[10] = max_y; feats[11] = max_z;
  feats[12] = max_x - min_x; feats[13] = max_y - min_y; feats[14] = max_z - min_z;

  // Normalizasyon
  float scaled_feats[15];
  for (int i = 0; i < 15; i++) {
    scaled_feats[i] = (feats[i] - SCALER_MEAN[i]) / SCALER_SCALE[i];
  }

  // Katman 1 (Dense 8 + ReLU)
  float h[8];
  for (int j = 0; j < 8; j++) {
    float val = BIAS0[j];
    for (int i = 0; i < 15; i++) {
      val += scaled_feats[i] * W0[i][j];
    }
    h[j] = val > 0 ? val : 0;
  }

  // Katman 2 (Dense 3 + Softmax)
  float logits[3];
  float max_l = -1e9;
  for (int c = 0; c < 3; c++) {
    float val = BIAS1[c];
    for (int j = 0; j < 8; j++) {
      val += h[j] * W1[j][c];
    }
    logits[c] = val;
    if (val > max_l) max_l = val;
  }

  float exp_sum = 0;
  float probs[3];
  for (int c = 0; c < 3; c++) {
    probs[c] = exp(logits[c] - max_l);
    exp_sum += probs[c];
  }

  int pred = 0;
  float best_prob = 0;
  for (int c = 0; c < 3; c++) {
    probs[c] /= exp_sum;
    if (probs[c] > best_prob) {
      best_prob = probs[c];
      pred = c;
    }
  }

  const char* class_names[3] = {"IDLE", "LEFTRIGHT", "UPDOWN"};
  Serial.print("Yapay Sinir Agi: [");
  Serial.print(class_names[pred]);
  Serial.print("]  Guven: %");
  Serial.println(best_prob * 100.0f, 1);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);
  STM32_Serial.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);

  Serial.println("\n==========================================");
  Serial.println("  ESP32-S3 TINYML NEURAL NETWORK INFERENCE");
  Serial.println("==========================================");
  Serial.println("Kayar pencere aktif (Gecikme: ~200 ms)...\n");
}

void loop() {
  if (STM32_Serial.available()) {
    String line = STM32_Serial.readStringUntil('\n');
    line.trim();

    if (line.length() > 0) {
      int c1 = line.indexOf(',');
      int c2 = line.indexOf(',', c1 + 1);

      if (c1 > 0 && c2 > c1) {
        int16_t x = line.substring(0, c1).toInt();
        int16_t y = line.substring(c1 + 1, c2).toInt();
        int16_t z = line.substring(c2 + 1).toInt();

        if (buffer_count < WINDOW_SIZE) {
          buf_x[buffer_count] = x;
          buf_y[buffer_count] = y;
          buf_z[buffer_count] = z;
          buffer_count++;
        } else {
          for (int i = 0; i < WINDOW_SIZE - 1; i++) {
            buf_x[i] = buf_x[i + 1];
            buf_y[i] = buf_y[i + 1];
            buf_z[i] = buf_z[i + 1];
          }
          buf_x[WINDOW_SIZE - 1] = x;
          buf_y[WINDOW_SIZE - 1] = y;
          buf_z[WINDOW_SIZE - 1] = z;

          step_counter++;
          if (step_counter >= STEP_SIZE) {
            step_counter = 0;
            run_neural_network();
          }
        }
      }
    }
  }
}
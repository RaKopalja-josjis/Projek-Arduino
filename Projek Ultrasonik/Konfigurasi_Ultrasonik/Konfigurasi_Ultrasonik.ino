const int trigPin = 6;
const int echoPin = 7;

void setup() {
  Serial.begin(9600);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  Serial.println("=== Mulai Test HY-SRF05 ===");
}

void loop() {
  // Pastikan Trig bersih sebelum memicu
  digitalWrite(trigPin, LOW);
  delayMicroseconds(5);

  // Kirim pulsa 10 mikrodetik
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Baca durasi Echo dengan timeout 30.000 us (~5 meter)
  long duration = pulseIn(echoPin, HIGH, 30000);

  if (duration == 0) {
    Serial.println("ERROR: Timeout (Tidak ada sinyal Echo / Kabel bermasalah)");
  } else {
    float distance = (duration * 0.0343) / 2.0;
    Serial.print("Durasi: ");
    Serial.print(duration);
    Serial.print(" us | Jarak: ");
    Serial.print(distance, 1);
    Serial.println(" cm");
  }

  delay(500);
}

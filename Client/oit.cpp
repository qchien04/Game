void setup() {
    for (int i = 0; i < 8; i++) {
      pinMode(i, OUTPUT);
      digitalWrite(i, LOW); 
    }
  }
  
  void OnLeftToRight(){
    for(int i = 7 ; i >= 0; i--){
      digitalWrite(i, HIGH);
      delay(200);
    }
  }
  
  void OffRightToLeft(){
    for(int i=0 ; i < 8; i++){
      digitalWrite(i, LOW);
      delay(200);
    }
  }
  
  void FirstOn(){
    for(int i = 7 ; i >= 4; i--){
      digitalWrite(i, HIGH);
    }
    for(int i = 3 ; i >= 0; i--){
      digitalWrite(i, LOW);
    }
    delay(1000);
  }
  
  void SecondOn(){
    for(int i = 7 ; i >= 4; i--){
      digitalWrite(i, LOW);
    }
    for(int i = 3 ; i >= 0; i--){
      digitalWrite(i, HIGH);
    }
    delay(1000);
  }
  
  void AllOn(){
    for(int i=7 ; i >= 0; i--){
      digitalWrite(i, HIGH);
    }
    delay(1500);
  }

  void AllOff(){
    for(int i=7 ; i >= 0; i--){
      digitalWrite(i, LOW);
    }
    delay(1000);
  }
  
  void loop() {
    OnLeftToRight();
    OffRightToLeft();
    FirstOn();
    SecondOn();
    AllOn();
    AllOff();
  }
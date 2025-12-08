int tim = 300;

void StartUP(){
    for (int i = 0; i < 2; i++) {     // number of beeps
      // short beep (active buzzer = beep beep)
      for (int j = 0; j < 300; j++) {
        digitalWrite(Buzz, HIGH);
        delayMicroseconds(100);
        digitalWrite(Buzz, LOW);
        delayMicroseconds(100);
      }
      delay(200);                     // pause between beeps
    }
  // --- End of startup sound ---
}

void LoggedIN(){
  
  for (int i = 0; i < 1; i++) {     // number of beeps
    // short beep (active buzzer = beep)
    for (int j = 0; j < 800; j++) {
      digitalWrite(Buzz, HIGH);
      delayMicroseconds(100);
      digitalWrite(Buzz, LOW);
      delayMicroseconds(100);
    }
    delay(200);                     // pause between beeps
  }
  // --- End of Logged sound ---
}

void LogInFailed(){
   for (int i = 0; i < 3; i++) {     // number of beeps
    // short beep (active buzzer = beep beep beeeeeep)
    if(i==2){
       tim = 1500;
    }
    else{ tim = 300;}
    
    for (int j = 0; j < tim; j++) {
      digitalWrite(Buzz, HIGH);
      delayMicroseconds(100);
      digitalWrite(Buzz, LOW);
      delayMicroseconds(100);
    }
    delay(200);                     // pause between beeps
  }
}

void ErrorBeep(){
    for (int j = 0; j < 5000; j++) {
      digitalWrite(Buzz, HIGH);
      delayMicroseconds(100);
      digitalWrite(Buzz, LOW);
      delayMicroseconds(100);
    }
}

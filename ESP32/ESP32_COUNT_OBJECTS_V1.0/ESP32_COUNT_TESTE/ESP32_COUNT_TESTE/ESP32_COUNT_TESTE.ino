bool s_high = 0;
unsigned long count = 0;

void setup() 
{
  Serial.begin(115200);
  pinMode(18, INPUT_PULLUP);
  
}

void loop() 
{
  if(digitalRead(18))   {

      s_high = 1;     
      
  }

  if (!digitalRead(18) && s_high == 1) {

    s_high = 0;
    count++;
    Serial.println(count);    
    delay(100);
  } 
  
}

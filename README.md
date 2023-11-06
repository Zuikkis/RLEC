
Think City Enerdel RLEC tools

Ingredients:
   * Rapsberry Pi (I think any model will do, I used pi3)
   * WaveShare CAN/RS485 hat


This is just some "quick&dirty" mashup, but it works. :) 

"rlec" program shows all connected RLECs, and their cell voltages and
temperatures. It can also set the balance target voltage.

Most RLEC documentation say that the cards need to be fed with constant
flow of CAN messages every 100ms or so. But this is not the case. It
works just fine if you just set the balance voltage once, then come back
tomorrow to check how good they are now. Or run the program in cronjob
to get some log every minute.

"rlecid" is a program to change RLEC card id. This is needed if you fix
a battery and need to combine RLEC cards from multiple cars with 
overlapping IDs.

Most of the CAN code is heavily based on WaveShare examples. 

RLEC information gathered mainly from:
https://www.metricmind.com/audi/main.htm

RLEC ID security protocol copied from SavvyCAN documentation:
https://www.savvycan.com/docs/scriptingwindow.html



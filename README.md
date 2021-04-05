# lets-talk

## Terminal to Terminal chat using Sockets created in C for the Linux Operating System

Type **make** in the directory to create an executable.  
Run the executable by typing: ./lets-talk (your port) (remote address) (remote port)  
For the other person to connect, they need to type: ./lets-talk (remote port) (your address) (your port)  

Check if the other person is online by typing: !status

ex.  
Person 1: ./lets-talk 3000 1.1.1.1 3001  
Person 2: ./lets-talk 3001 2.2.2.2 3000  

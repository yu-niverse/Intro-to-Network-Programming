#!/bin/bash

make 

SERVER_SESSION="np_demo_server"
CLIENT_SESSION="np_demo_client"

if [ -n "`tmux ls | grep $SERVER_SESSION`" ]; then
  tmux kill-session -t $SERVER_SESSION
fi

if [ -n "`tmux ls | grep $CLIENT_SESSION`" ]; then
  tmux kill-session -t $CLIENT_SESSION
fi

IP=$1
PORT=$2
SLEEP_TIME=2

# Create the server
tmux new-session -d -s $SERVER_SESSION
tmux set remain-on-exit on

tmux send-keys -t $SERVER_SESSION "./server $PORT" ENTER

# Create the clients
tmux new-session -d -s $CLIENT_SESSION
tmux set remain-on-exit on

tmux select-pane -t 0
tmux split-window -h

sleep $SLEEP_TIME

# Connection test
echo -e "\nconnection & exit test...\c"

for i in $(seq 0 1)
do
    tmux send-keys -t ${i} "./client $IP $PORT >> user${i}.txt" Enter
    sleep $SLEEP_TIME

    # You must ensure that your server is running after the connection closeing
    # Also, the client should close properly
    tmux send-keys -t ${i} "exit" Enter          
    sleep $SLEEP_TIME

done

for i in $(seq 0 1)
do
    # No difference with connection.txt is correct
    diff user${i}.txt ./connection.txt -b -B -y --suppress-common-lines >> test_connection.txt
done

# If there's difference between your client output and connection.txt, the difference will be printed:
if [ -s test_connection.txt ]; then
  echo " Wrong"
  cat test_connection.txt
else
  echo " Success"
fi

rm user*


# Game-rule test
echo -e "\ngame-rule test...\c"

for i in $(seq 0 1)
do
    tmux send-keys -t ${i} "./client $IP $PORT >> user${i}.txt" Enter
    sleep $SLEEP_TIME
          
    tmux send-keys -t ${i} "game-rule" Enter
    sleep $SLEEP_TIME  

    tmux send-keys -t ${i} "exit" Enter          
    sleep $SLEEP_TIME

done

for i in $(seq 0 1)
do
    # No difference with game-rule.txt is correct
    diff user${i}.txt ./game-rule.txt -b -B -y --suppress-common-lines >> test_gamerule.txt
done

# If there's difference between your client output and game-rule.txt, the difference will be printed:
if [ -s test_gamerule.txt ]; then
  echo " Wrong"
  cat test_gamerule.txt
else
  echo " Success"
fi

# If you want to see how your program is running on tmux,
# you can comment out these two commands and just use
# "tmux attach -t np_demo_server" or "tmux attach -t np_demo_client"
# to check server and client respectively
tmux kill-session -t $SERVER_SESSION
tmux kill-session -t $CLIENT_SESSION


# Delete the executable files
rm server
rm client
rm user*
rm test_*
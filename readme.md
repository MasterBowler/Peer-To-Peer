## How to compile
Run make in the commnd line interface.
```
make
```

## How to run
Run the following in the command line interface.
```
./part2part
```

## How to use
All the following commands are for use in the command line interface.

1. connect \<IP\> \<PORT\> <br/>
Example: ```connect 10.0.2.15 35678``` <br/>
Use this to connect to a peer and join it's network.
2. add \<filename\> <br/>
Example: ```add ex.txt``` <br/>
Use this to share a file.
3. remove \<filename\> <br/>
Example: ```remove ex.txt``` <br/>
Use this to stop sharing a file.
4. search \<filename\> <br/>
Example: ```search ex.txt``` <br/>
Use this to find a file.
5. download \<filename\> \<IP\> \<PORT\> <br/>
Example: ```download ex.txt 10.0.2.15 35678``` <br/>
Use this to download a file after search returns one or more peers that have it.
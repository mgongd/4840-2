# TODO:
## Complete the network communication
- When your client receives a packet from the server, display it on the next line
at the **top** of the screen.
    - `fbputchunk()` displays a buffer, with automatic line wrapping
- When the user presses **return**, have your client send to the server the text s/he
has been typing **and** display it in the text area at the top of the sceen.
    - Clear the bottom area

## Complete Editor Interface
- Make left and right arrow keys work
    - I use `cursor` to store the current cursor position in the input buffer
    - `insert()` and `delete()` inserts and deletes at the cursor position
- Make cursor display work
    - `fbputcursor()` displays character **inverted** (cursor)
    - Maybe modify `fbputchunk()` to display the cursor as well

# FPGA Setup
```shell
ifup eth0
stty rows 74
stty cols 132
```
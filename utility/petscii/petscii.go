package main

import (
    "fmt"
    "image/gif"
    "log"
    "os"
)

func main() {
    charset, err := os.Open("characters.901225-01.gif")
    if err != nil {
        log.Fatal(err)
    }
    defer charset.Close()
    imageData, err := gif.Decode(charset)
    if err != nil {
        log.Fatal(err)
    }
    var i = 0;
    for row := 0; row < 32; row++ {
        for col := 0; col < 16; col++ {
            for y := 0; y < 8; y++ {
                var c = 0;
                for x := 0; x < 8; x++ {
                    r, _, _, _ := imageData.At(col * 8 + x, row * 8 + y).RGBA();
                    if r == 65535 {
                        c=c|1<<(7-x)
                    }
                }
                fmt.Printf("0x%02x, ", c);
            }   
            fmt.Printf(" //%d\n", i);
            i++;
        }
    }
}

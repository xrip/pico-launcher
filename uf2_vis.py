

f=open("/home/alex/Рабочий стол/APPLICATION128.uf2","rb")


fdata=f.read()

f.close()

inx=1
for i in range(len(fdata)):
            if i%512==0:
                        print("\n      inx="+str(inx))
                        inx+=1
                        bsize=int.from_bytes(fdata[i+16:i+20],"little")
                        binx=int.from_bytes(fdata[i+20:i+24],"little")
                        bN=int.from_bytes(fdata[i+24:i+28],"little")
                        baddr=int.from_bytes(fdata[i+12:i+16],"little")
                        bId=int.from_bytes(fdata[i+28:i+32],"little")
                        print("size="+str(bsize))
                        print("block= "+str(binx)+" ,total= "+str(bN))
                        print("addr= "+hex(baddr))
                        print("Id= "+hex(bId))
                        
                        print(hex(int.from_bytes(fdata[i:i+4],"little")))       

print("size= "+str(bN*256))
                

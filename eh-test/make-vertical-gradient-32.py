from PIL import Image
max_x = 256
max_y = 256
max_level = 31
im = Image.new('L', (max_x, max_y), None)
pix = im.load()
for y in range(max_y):
    for x in range(max_x):
        pix[x, y] = int(max_level * float(max_y-y)/max_y)
im.save('vertical-gradient-32-256x256.png')

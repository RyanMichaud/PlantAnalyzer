
#This program is free software: you can redistribute it and/or modify
#it under the terms of the GNU General Public License as published by
#the Free Software Foundation, either version 3 of the License, or
#(at your option) any later version.

#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.

#You should have received a copy of the GNU General Public License
#along with this program.  If not, see <http://www.gnu.org/licenses/>.

maxwidth=500.
maxheight=500.

from app import app
import os
from flask import Flask, render_template, send_from_directory, send_file, request, url_for, jsonify, redirect, Request, g
from cStringIO import StringIO
from werkzeug import secure_filename
from azure.storage.blob import BlockBlobService, ContentSettings

#block_blob_service = BlockBlobService(account_name='xxxxxxxxxxxx', account_key='xxxxxxxxxxxxxxxxx')

os.environ[ 'MPLCONFIGDIR' ] = '/tmp/'

import matplotlib
matplotlib.use('Agg')
#import pylab as pl

#import numpy
import matplotlib.image as mpimg
import matplotlib.pyplot as plt
import numpy as numpy

from PIL import Image, ImageDraw, ImageFont
import gc

UPLOAD_FOLDER = os.path.join(app.root_path, 'uploads')
NDVI_FOLDER = os.path.join(app.root_path, 'ndvi')
STATIC_FOLDER = os.path.join(app.root_path, 'static')

ALLOWED_EXTENSIONS = set(['png', 'jpg', 'jpeg', 'gif','PNG','JPEG','JPG'])

app.config['UPLOAD_FOLDER'] = UPLOAD_FOLDER
app.config['NDVI_FOLDER'] = NDVI_FOLDER
app.config['STATIC_FOLDER'] = STATIC_FOLDER
app.config['MAX_CONTENT_LENGTH'] = 16*1024*1024
app.debug = True
app.static_folder = 'static'
app.static_url_path = ''

#@app.errorhandler(413)
#def file_to_big(e):
#    return render_template('tooBig.html')

@app.route('/sandbox')
def sandbox():
    return send_from_directory('/static/sandbox', 'index.html')

def nir(imageInPath,imageOutPath):
    img=Image.open(imageInPath)
    imgR, imgG, imgB = img.split() #get channels
    arrR = numpy.asarray(imgR).astype('float64')

    arr_nir=arrR

    img_w,img_h=img.size

    dpi=600. #need this to be floating point!
    fig_w=img_w/dpi
    fig_h=img_h/dpi

    fig=plt.figure(figsize=(fig_w,fig_h),dpi=dpi)

    fig.set_frameon(False)

    ax_rect = [0.0, #left
           0.0, #bottom
           1.0, #width
           1.0] #height
    ax = fig.add_axes(ax_rect)
    ax.yaxis.set_ticklabels([])
    ax.xaxis.set_ticklabels([])   
    ax.set_axis_off()
    ax.axes.get_yaxis().set_visible(False)
    ax.patch.set_alpha(0.0)

    axes_img = ax.imshow(arr_nir,
              cmap=plt.cm.gist_gray,
              aspect = 'equal',
              interpolation="nearest"
             )

    fig.savefig(imageOutPath, 
            dpi=dpi,
            bbox_inches='tight',
            pad_inches=0.0, 
           )

    #plt.show()  #show the plot after saving
    fig.clf()
    plt.close()
    gc.collect()

def only_blue(imageInPath,imageOutPath):
    img=Image.open(imageInPath)
    imgR, imgG, imgB = img.split() #get channels
    arrB = numpy.asarray(imgB).astype('float64')

    arr_only_blue=arrB

    img_w,img_h=img.size

    dpi=600. #need this to be floating point!
    fig_w=img_w/dpi
    fig_h=img_h/dpi

    fig=plt.figure(figsize=(fig_w,fig_h),dpi=dpi)

    fig.set_frameon(False)

    ax_rect = [0.0, #left
           0.0, #bottom
           1.0, #width
           1.0] #height
    ax = fig.add_axes(ax_rect)
    ax.yaxis.set_ticklabels([])
    ax.xaxis.set_ticklabels([])   
    ax.set_axis_off()
    ax.axes.get_yaxis().set_visible(False)
    ax.patch.set_alpha(0.0)

    axes_img = ax.imshow(arr_only_blue,
              cmap=plt.cm.gist_gray,
              aspect = 'equal',
              interpolation="nearest"
             )

    fig.savefig(imageOutPath, 
            dpi=dpi,
            bbox_inches='tight',
            pad_inches=0.0, 
           )

    #plt.show()  #show the plot after saving
    fig.clf()
    plt.close()
    gc.collect()

def ndvi(imageInPath,imageOutPath):
    img=Image.open(imageInPath)
    imgR, imgG, imgB = img.split() #get channels
    arrR = numpy.asarray(imgR).astype('float64')
    arrB = numpy.asarray(imgB).astype('float64')
    
    num=arrR - arrB
    num=(arrR - arrB)
    denom=(arrR + arrB)
    arr_ndvi=num/denom

    vmin=-1.
    vmax = 1.

    img_w,img_h=img.size

    dpi=600. #need this to be floating point!
    fig_w=img_w/dpi
    fig_h=img_h/dpi

    fig=plt.figure(figsize=(fig_w,fig_h),dpi=dpi,linewidth=0.3)

    fig.set_frameon(False)

    ax_rect = [0.0, #left
           0.0, #bottom
           1.0, #width
           1.0] #height
    ax = fig.add_axes(ax_rect)
    ax.yaxis.set_ticklabels([])
    ax.xaxis.set_ticklabels([])   
    ax.set_axis_off()
    ax.axes.get_yaxis().set_visible(False)
    ax.patch.set_alpha(0.0)

    cdict = {
    'red'  :  ((0.00, 0.00, 0.00), 
               (0.20, 0.00, 0.00),
               (0.50, 0.00, .00),
               (0.70, 1.00, 1.00),
               (1.00, 1.00, 1.00)),
    'green':  ((0.00, 0.00, 0.00), 
               (0.20, 0.00, 0.00),
               (0.50, 1.00,.50),
               (0.70, 1.00, 1.00),
               (1.00, 0.00, 0.00)),
    'blue' :  ((0.00, 0.00, 0.00), 
               (0.20, 1.00, 1.00),
    #           (0.40, 0.00, 0.00),
               (0.50, 1.00, 0.00),
               (0.70, 0.00, 0.00), 
               (1.00, 0.00, 0.00)),
    }


    fastie_cmap = matplotlib.colors.LinearSegmentedColormap('my_colormap',cdict,256)


    axes_img = ax.imshow(arr_ndvi,
              cmap=fastie_cmap,
              aspect = 'equal',
              interpolation="nearest",
              vmin=-1.0,
              vmax=1.0
             )

    # Add colorbar 
    #make an axis for colorbar
    #cax = fig.add_axes([0.8,0.05,0.05,0.85]) #left, bottom, width, height
    #cbar = fig.colorbar(axes_img, cax=cax)  #this resizes the axis
    #cbar=fig.colorbar(axes_img)
    #cbar.ax.tick_params(length=1, labelsize=2) #this changes the font size on the axis

    fig.savefig(imageOutPath, 
            dpi=dpi,
            bbox_inches='tight',
            pad_inches=0.0, 
           )

    #plt.show()  #show the plot after saving
    fig.clf()
    
    fastie_cmap = matplotlib.colors.LinearSegmentedColormap('my_colormap',cdict,256)
    
    a = numpy.array([[-1,1]])
    plt.figure(figsize=(9, 1.5))
    img = plt.imshow(a, cmap=fastie_cmap)
    plt.gca().set_visible(False)
    cax = plt.axes([0.1, 0.2, 0.8, 0.6])
    plt.colorbar(orientation="h", cax=cax)
    colorbarFilepath=os.path.join(app.config['UPLOAD_FOLDER'],'colorbar.png')
    plt.savefig(colorbarFilepath)


    img=Image.open(colorbarFilepath)
    colorthumbFilePath=os.path.join(app.config['UPLOAD_FOLDER'],"colorbar_thumb.png")
    width,height=img.size
    #new_width=maxwidth*.8
    new_width=img_w*.8
    ratio=new_width/width
    new_height=ratio*height
    colorbar_size=new_width,new_height
    img.thumbnail(colorbar_size,Image.ANTIALIAS)
    img.save(colorthumbFilePath)

    infragramLogoFilepath=os.path.join(app.static_folder,"img/infragram-mini-leaf.png")
    logoThumbFilePath=os.path.join(app.config['UPLOAD_FOLDER'],"infragramLogo_thumb.png")
    img=Image.open(infragramLogoFilepath)
    width,height=img.size
    #new_width=maxwidth*.2
    new_width=img_w*.2
    ratio=new_width/width
    new_height=ratio*height
    logo_size=new_width,new_height
    img.thumbnail(logo_size,Image.ANTIALIAS)
    img.save(logoThumbFilePath)
    
 
    new_im=Image.new('RGB',(int(colorbar_size[0]+logo_size[0]),int(max(colorbar_size[1],logo_size[1]))),(255,255,255))
    imLogo=Image.open(logoThumbFilePath)
    imColorbar=Image.open(colorthumbFilePath)
    new_im.paste(imLogo,(0,0))
    new_im.paste(imColorbar,(int(logo_size[0]),0))
    fullBarFilepath=os.path.join(app.config['UPLOAD_FOLDER'],"fullBar.png")
    new_im.save(fullBarFilepath)
    
    new_im_width,new_im_height=new_im.size
    composite=Image.new('RGB',(img_w,img_h+new_im_height),(255,255,255))
    mainImage=Image.open(imageOutPath)
    composite.paste(mainImage,(0,0))
    composite.paste(new_im,(0,img_h))
    
    caption = Image.new("RGBA", (1200,90), (255,255,255))
    draw = ImageDraw.Draw(caption)
    fontsize=36
    filename='test.png'
    fontpath=os.path.join(app.static_folder,"fonts/helvnarrow.ttf")
    font = ImageFont.truetype(fontpath, fontsize)
    parts=imageOutPath.split("/")
    fileName=parts[len(parts)-1]
    txt='http://infragram.org/show/'+fileName
    draw.text((10, 0), txt, (0,0,0), font=font)
    img_resized=caption.resize((400,30),Image.ANTIALIAS)
    #img_resized.save('testtext.png')

    composite_w, composite_h=composite.size
    caption_height=composite_h-15
    caption_width=int(logo_size[0])+int(colorbar_size[0]/2)-100
    composite.paste(img_resized,(caption_width,caption_height))
    #compositeFilePath=os.path.join(app.config['UPLOAD_FOLDER'],"composite.png")
    composite.save(imageOutPath)
    plt.close()
    gc.collect()
    
def allowed_file(filename):
    return '.' in filename and \
           filename.rsplit('.', 1)[1] in ALLOWED_EXTENSIONS

def isblue(filename):
    return '.' in filename and \
           'BLUE' in filename.rsplit('.', 1)[0]

def notblue(filename):
    return '.' in filename and \
           'NORM' in filename.rsplit('.', 1)[0]

@app.route('/', methods=['GET', 'POST'])
def upload_file():
    if request.method=='POST':
        file=request.files['file']
        if (file and allowed_file(file.filename) and notblue(file.filename)):
            filename=secure_filename(file.filename)
            uploadFilePath=os.path.join(app.config['UPLOAD_FOLDER'],filename)
            file.save(uploadFilePath)
            img=Image.open(uploadFilePath)           
            width,height=img.size
            size=width,height
            block_blob_service.create_blob_from_path('normalpicture', filename, uploadFilePath +'/'+filename, content_settings=ContentSettings(content_type='image/jpg'))
            return render_template('render.html')
        elif (file and allowed_file(file.filename) and isblue(file.filename)):
            filename=secure_filename(file.filename)
            uploadFilePath=os.path.join(app.config['UPLOAD_FOLDER'],filename)
            file.save(uploadFilePath)
            img=Image.open(uploadFilePath)           
            width,height=img.size
            size=width,height
            ndviFilePath=os.path.join(app.config['UPLOAD_FOLDER'],'ndvi_'+filename)
            nirFilePath=os.path.join(app.config['UPLOAD_FOLDER'],'nir_'+filename)
            #blueFilePath=os.path.join(app.config['UPLOAD_FOLDER'],'blue_'+filename)
            ndvi(uploadFilePath,ndviFilePath)
            nir(uploadFilePath,nirFilePath)
            #only_blue(uploadFilePath,blueFilePath)
            block_blob_service.create_blob_from_path('infrabluepicture', filename, uploadFilePath + '/' + filename, content_settings=ContentSettings(content_type='image/jpg'))
            block_blob_service.create_blob_from_path('ndvipicture', 'ndvi_'+filename, uploadFilePath + 'ndvi_' + filename, content_settings=ContentSettings(content_type='image/jpg'))
            return render_template('render.html')  
    return render_template('index.html')

@app.route('/uploads/<filename>')
def send_file(filename):
    return send_from_directory(UPLOAD_FOLDER,filename)

@app.route('/show/<filename>')
def uploaded_file(filename):
    uploadFilePath=os.path.join(app.config['UPLOAD_FOLDER'],filename)
    ndviFilePath=os.path.join(app.config['NDVI_FOLDER'],filename)  
    #return render_template('render.html',filename='/uploads/'+'thumb_'+filename,
    return render_template('render.html',filename='/uploads/'+filename, ndviFilename='/uploads/'+'ndvi_'+filename, nirFilename='/uploads/'+'nir_'+filename, blueFilename='/uploads/'+'blue_'+filename)
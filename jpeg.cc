#include <node.h>
#include <node_buffer.h>
#include <jpeglib.h>
#include <cstdlib>
#include <cstring>

using namespace v8;
using namespace node;

static Handle<Value>
VException(const char *msg) {
    HandleScope scope;
    return ThrowException(Exception::Error(String::New(msg)));
}

typedef enum { BUF_RGBA, BUF_RGB } buf_type_t;

class Jpeg : public ObjectWrap {
private:
    int width_;
    int height_;
    int quality_;
    buf_type_t buf_type_;
    Buffer *rgba_;

public:
    static void
    Initialize(v8::Handle<v8::Object> target)
    {
        HandleScope scope;

        Local<FunctionTemplate> t = FunctionTemplate::New(New);
        t->InstanceTemplate()->SetInternalFieldCount(1);
        NODE_SET_PROTOTYPE_METHOD(t, "encode", JpegEncode);
        target->Set(String::NewSymbol("Jpeg"), t->GetFunction());
    }

    Jpeg(Buffer *rgba, int width, int height, int quality, buf_type_t buf_type) :
        rgba_(rgba), width_(width), height_(height), quality_(quality),
        buf_type_(buf_type) {}

    static unsigned char *
    rgba_to_rgb(unsigned char *rgba, int rgba_size)
    {
        int rgb_size = rgba_size/4*3;
        unsigned char *rgb = (unsigned char *)malloc(sizeof(unsigned char)*rgb_size);
        if (!rgb) return NULL;
        int i, j;
        for (i=0,j=0;i<rgba_size;i+=4,j+=3) {
            rgb[j] = *(rgba+i);
            rgb[j+1] = *(rgba+i+1);
            rgb[j+2] = *(rgba+i+2);
        }
        return rgb;
    }

    Handle<Value> JpegEncode() {
        HandleScope scope;

        unsigned char *mem_dest;
        unsigned long outsize = 0;

        struct jpeg_compress_struct cinfo;
        struct jpeg_error_mgr jerr;

        cinfo.err = jpeg_std_error(&jerr);

        jpeg_create_compress(&cinfo);
        jpeg_mem_dest(&cinfo, &mem_dest, &outsize);

        cinfo.image_width = width_;
        cinfo.image_height = height_;
        cinfo.input_components = 3;
        cinfo.in_color_space = JCS_RGB;

        jpeg_set_defaults(&cinfo);
        jpeg_set_quality(&cinfo, quality_, TRUE);
        jpeg_start_compress(&cinfo, TRUE);

        unsigned char *rgb_data;
        if (buf_type_ == BUF_RGBA) {
            rgb_data = rgba_to_rgb((unsigned char *)rgba_->data(), width_*height_*4);
            if (!rgb_data) return VException("malloc failed in rgba_to_rgb.");
        } else {
            rgb_data = (unsigned char *)rgba_->data();
        }

        JSAMPROW row_pointer;
        while (cinfo.next_scanline < cinfo.image_height) {
            row_pointer = &rgb_data[cinfo.next_scanline*3*width_];
            jpeg_write_scanlines(&cinfo, &row_pointer, 1);
        }

        jpeg_finish_compress(&cinfo);
        jpeg_destroy_compress(&cinfo);

        free(mem_dest);
        if (buf_type_ == BUF_RGBA) free(rgb_data);

        return scope.Close(Encode((char *)mem_dest, outsize, BINARY));
    }

protected:
    static Handle<Value>
    New(const Arguments& args)
    {
        HandleScope scope;

        if (args.Length() != 5)
            return VException("Five arguments required - rgba/rgb buffer, width, height, quality, buffer type");
        if (!Buffer::HasInstance(args[0]))
            return VException("First argument must be Buffer.");
        if (!args[1]->IsInt32())
            return VException("Second argument must be integer width.");
        if (!args[2]->IsInt32())
            return VException("Third argument must be integer height.");
        if (!args[3]->IsInt32())
            return VException("Fourth argument must be integer quality.");
        if (!args[4]->IsString())
            return VException("Fifth argument must be a string. Either 'rgba' or 'rgb'.");

        int w = args[1]->Int32Value();
        int h = args[2]->Int32Value();
        int quality = args[3]->Int32Value();
        String::AsciiValue bt(args[4]->ToString());

        if (quality < 0 || quality > 100)
            return VException("Quality must be between 0 and 100 (inclusive).");
        if (!(strcmp(*bt, "rgb") == 0 || strcmp(*bt, "rgba") == 0))
            return VException("Fifth argument must be either 'rgba' or 'rgb'.");
        buf_type_t buf_type = (strcmp(*bt, "rgb") == 0) ?  BUF_RGB : BUF_RGBA;

        Buffer *rgba = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());
        Jpeg *jpeg = new Jpeg(rgba, w, h, quality, buf_type);
        jpeg->Wrap(args.This());
        return args.This();
    }

    static Handle<Value>
    JpegEncode(const Arguments& args)
    {
        HandleScope scope;
        Jpeg *jpeg = ObjectWrap::Unwrap<Jpeg>(args.This());
        return jpeg->JpegEncode();
    }
};

extern "C" void
init(Handle<Object> target)
{
    HandleScope scope;
    Jpeg::Initialize(target);
}


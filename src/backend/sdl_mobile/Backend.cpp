#include "backend/sdl_mobile/Backend.h"
#include "FlxG.h"
#include <SDL_opengles2.h>

// SDL2 hack starts here
extern "C" {
	#include "../../SDL/src/render/SDL_sysrender.h"
	
	typedef struct GLES2_TextureData
	{
		GLenum texture;
		GLenum texture_type;
		GLenum pixel_format;
		GLenum pixel_type;
		void *pixel_data;
		size_t pitch;
		void *fbo;
	} GLES2_TextureData; 
	
	GLenum SDL_GLES2_GetTextureID(SDL_Texture* tex) {
		GLES2_TextureData *texture = (GLES2_TextureData*) tex->driverdata;
		return texture->texture;
	};
}


// default vertex shader
const GLchar DefaultVertexShader[] = \
    "attribute vec2 a_position;\n"
    "attribute vec2 a_texCoord;\n"
    "varying vec2 TexCoords;\n"
    "\n"
    "void main() {\n"
        "TexCoords = a_texCoord;\n"
        "gl_Position = vec4(a_position, 0.0, 1.0);\n"
    "}\n";


/*
*  SDL image class
*/
class SDL_Image : public FlxBackendImage {

public:
    SDL_Texture *texture;
	int width, height;

    int getWidth() {
        return width;
    }

    int getHeight() {
        return height;
    }

    int getFormat() {
        return 0; // default
    }
};


/*
*  SDL shader class
*/
class SDL_Shader : public FlxBackendShader {

public:
    GLint shaderProgram, vertexShader, fragmentShader;
    std::map<std::string, GLint> textures;

    virtual ~SDL_Shader() {
        glDeleteShader(fragmentShader);
		glDeleteShader(vertexShader);
		glDeleteProgram(shaderProgram);
    }

    virtual void setParameter(const char *name, float x) {
        glUseProgram(shaderProgram);
		GLint location = glGetUniformLocation(shaderProgram, name);

		if(location != -1) {
		    glUniform1f(location, x);
        }

		glUseProgram(0);
    }

    virtual void setParameter(const char *name, float x, float y) {
        glUseProgram(shaderProgram);
		GLint location = glGetUniformLocation(shaderProgram, name);

		if(location != -1) {
		    glUniform2f(location, x, y);
        }

		glUseProgram(0);
    }

    virtual void setParameter(const char *name, float x, float y, float z) {
        glUseProgram(shaderProgram);
		GLint location = glGetUniformLocation(shaderProgram, name);

		if(location != -1) {
		    glUniform3f(location, x, y, z);
        }

		glUseProgram(0);
    }

    virtual void setParameter(const char *name, float x, float y, float z, float w) {
        glUseProgram(shaderProgram);
		GLint location = glGetUniformLocation(shaderProgram, name);

		if(location != -1) {
		    glUniform4f(location, x, y, z, w);
        }

		glUseProgram(0);
    }

    virtual void setParameter(const char *name, FlxBackendImage *i) {
        if(i) {
            SDL_Image *img = (SDL_Image*) i;
            textures[name] = (int) SDL_GLES2_GetTextureID(img->texture);
        }
        else {
            textures[name] = -1;
        }
    }
};


class SDL_File : public FlxBackendFile {

private:
    FILE *externalFilePtr;
	SDL_RWops *internalFilePtr;
	bool internalFilsystem;
public:
    virtual ~SDL_File() {
        close();
    }

	SDL_File() {
		externalFilePtr = NULL;
		internalFilePtr = NULL;
	}
	
    virtual bool open(const char *path, const char *mode, bool internal) {
		internalFilsystem = internal;
		
		if(!internalFilsystem) {
			externalFilePtr = fopen(path, mode);
			return externalFilePtr != NULL;
		}
		else {
			internalFilePtr = SDL_RWFromFile(path, mode);
			return internalFilePtr != NULL;
		}
    }

    virtual bool eof() {
		if(!internalFilsystem) {
			return feof(externalFilePtr);
		}
		else {
			unsigned int current = tell();
		    seek(0, SEEK_END);
			unsigned int size = tell();
			seek(current, SEEK_SET);
	
			return tell() == size;
		}
    }

    virtual unsigned int tell() {
		if(!internalFilsystem) {
			return ftell(externalFilePtr);
		}
		else {
			return SDL_RWtell(internalFilePtr);
		}
    }

    virtual void seek(long offset, int origin) {
		if(!internalFilsystem) {
			fseek(externalFilePtr, offset, origin);
		}
		else {
			SDL_RWseek(internalFilePtr, offset, origin);
		}
    }

    virtual void write(const char *data, unsigned int size) {
		if(!internalFilsystem) {
			fwrite(data, 1, size, externalFilePtr);
		}
		else {
			SDL_RWwrite(internalFilePtr, data, 1, size);
		}
    }

    virtual int read(char *data, unsigned int maxSize) {
		if(!internalFilsystem) {
			return fread(data, 1, maxSize, externalFilePtr);
		}
		else {
			return SDL_RWread(internalFilePtr, data, 1, maxSize);
		}
    }

    virtual void close() {
		if(!internalFilsystem) {
			if(externalFilePtr) fclose(externalFilePtr);
		}
		else {
			if(internalFilePtr) SDL_RWclose(internalFilePtr);
		}
    }
};


/*
*  SDL sound class
*/
class SDL_Sound : public FlxBackendSound {

public:
    Mix_Chunk *buffer;
	int channel;

    virtual ~SDL_Sound() {
        stop();
    }

    virtual void play() {

		if(isPlaying()) {
			stop();
		}

		channel = Mix_PlayChannel(-1, buffer, 0);
    }

    virtual void stop() {
		Mix_HaltChannel(channel);
		channel = -1;
    }

    virtual void setLoop(bool t) {
		// not implemented yet
    }

    virtual void setVolume(float vol) {
        if(buffer) Mix_VolumeChunk(buffer, int(vol * 128.f));
    }

    virtual bool isPlaying() {
        return Mix_Playing(channel) != 0;
    }
};


/*
*  SDL music holder
*/
class MusicHolder {

private:
	static Mix_Music *currentPlaying;
	static bool finished;
public:
	static void play(Mix_Music *chunk) {
		if(!finished) stop(chunk);

		finished = false;
		currentPlaying = chunk;

		Mix_HookMusicFinished(&MusicHolder::musicFinished);
		Mix_PlayMusic(currentPlaying, 0);
	}

	static void stop(Mix_Music *chunk) {
		if(chunk != currentPlaying) return;

		Mix_HaltMusic();
		finished = true;
		musicFinished();
	}

	static void setVolume(Mix_Music *chunk, int volume) {
		Mix_VolumeMusic(volume);
	}

	static bool isPlaying(Mix_Music *chunk) {
		return (chunk == currentPlaying && chunk != NULL);
	}

	static void musicFinished() {
		if(!finished) Mix_HaltMusic();

		finished = true;
		currentPlaying = NULL;
	}
};

Mix_Music *MusicHolder::currentPlaying = NULL;
bool MusicHolder::finished = true;


/*
*  SDL music class
*/
class SDL_Music : public FlxBackendMusic {

public:
	Mix_Music *buffer;

    virtual ~SDL_Music() {
        stop();
		Mix_FreeMusic(buffer);
    }

    virtual void play() {
		MusicHolder::play(buffer);
    }

    virtual void stop() {
		MusicHolder::stop(buffer);
    }

    virtual void setLoop(bool t) {
    }

    virtual void setVolume(float vol) {
        MusicHolder::setVolume(buffer, int(vol * 255.f));
    }

    virtual bool isPlaying() {
        return MusicHolder::isPlaying(buffer);
    }
};



/*
*  Main backend class definition
*/
bool SDL_Mobile_Backend::setupSurface(const char *title, int width, int height, const char*) {

    // initialize SDL
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

	// initialize addtional libraries
	IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
	TTF_Init();
	Mix_Init(MIX_INIT_OGG | MIX_INIT_MOD);
	SDLNet_Init();

	// get screen size
	SDL_DisplayMode mode;
	SDL_GetDesktopDisplayMode(0, &mode);
	screenWidth = mode.w;
	screenHeight = mode.h;
	
    // create the window
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    window = SDL_CreateWindow(title, 0, 0, width, height, SDL_WINDOW_OPENGL |
                              SDL_WINDOW_BORDERLESS | SDL_WINDOW_SHOWN);
	SDL_SetWindowFullscreen(window, SDL_TRUE);
	renderer = SDL_CreateRenderer(window, -1, 0);

	// open audio device
	int audio_rate = 22050;
	Uint16 audio_format = AUDIO_S16;
	int audio_channels = 2;
	int audio_buffers = 4096;

	Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers);

    for(int i = 0; i < 1024; i++) {
        keysDown[i] = false;
    }

    // create framebuffer if needed
    if(isShadersSupported()) {
        unsigned char *pixels = new unsigned char[width * height * 3];
        for(unsigned int i = 0; i < sizeof(pixels); i++) pixels[i] = 255;

        glGenTextures(1, &framebuffer);
		glBindTexture(GL_TEXTURE_2D, framebuffer);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D(GL_TEXTURE_2D, 0, 3, width, height, 0, GL_RGB,
               GL_UNSIGNED_BYTE, pixels);

        delete[] pixels;
    }

	exitMsg = false;
    return true;
}

void SDL_Mobile_Backend::mainLoop(void (*onUpdate)(), void (*onDraw)()) {

    float currentTime = (float)SDL_GetTicks() / 1000.f;
    float accumulator = 0;

    while(!FlxG::exitMessage && !exitMsg) {

        // fixed timestep stuff
        float newTime = (float)SDL_GetTicks() / 1000.f;
        float elapsed = newTime - currentTime;
        currentTime = newTime;

        accumulator += elapsed;

        while(accumulator >= FlxG::fixedTime) {

            // update all stuff
            updateEvents();
            onUpdate();

            accumulator -= FlxG::fixedTime;
        }

		SDL_SetRenderDrawColor(renderer, COLOR_GET_R(FlxG::bgColor), COLOR_GET_G(FlxG::bgColor),
                        COLOR_GET_B(FlxG::bgColor), 255);
		SDL_RenderClear(renderer);

		onDraw();

		SDL_RenderPresent(renderer);

        FlxG::elapsed = elapsed;
    }
}

FlxVector SDL_Mobile_Backend::getScreenSize() {
    return FlxVector((float)screenWidth, (float)screenHeight);
}

void SDL_Mobile_Backend::exitApplication() {

	Mix_HaltChannel(-1);
	Mix_HaltMusic();
    glDeleteTextures(1, &framebuffer);

	for(std::map<std::string, FlxBackendImage*>::iterator it = images.begin(); it != images.end(); it++) {
		if(it->second) {
			SDL_Image *img = (SDL_Image*)it->second;

			if(img->texture) SDL_DestroyTexture(img->texture);
			delete img;
		}
	}

	for(std::map<std::string, void*>::iterator it = fonts.begin(); it != fonts.end(); it++) {
		if(it->second) {
			TTF_Font *font = (TTF_Font*)it->second;
			if(font) TTF_CloseFont(font);
		}
	}

	for(std::map<std::string, void*>::iterator it = sounds.begin(); it != sounds.end(); it++) {
        Mix_Chunk *s = (Mix_Chunk*) it->second;
		if(s) Mix_FreeChunk(s);
    }

    for(unsigned int i = 0; i < shaders.size(); i++) {
        if(shaders[i]) delete shaders[i];
    }

	Mix_CloseAudio();
	SDLNet_Quit();
	Mix_Quit();
	TTF_Quit();
	IMG_Quit();
    SDL_Quit();
}

bool* SDL_Mobile_Backend::getKeysDown() {
    return keysDown;
}

bool SDL_Mobile_Backend::isKeyDown(int code) {
    return keysDown[code];
}

void SDL_Mobile_Backend::updateEvents() {

	SDL_Event event;
	while (SDL_PollEvent(&event)) {

		if(event.type == SDL_WINDOWEVENT_CLOSE || event.type == SDL_QUIT) {
			exitMsg = true;
        }

		// finger down
		else if(event.type == SDL_FINGERDOWN) {
			SDL_Touch *touch = SDL_GetTouch(event.tfinger.touchId);
			FlxMouse::onTouchBegin(event.tfinger.fingerId, ((float)event.tfinger.x / (float)touch->xres) * FlxG::width,
				((float)event.tfinger.y / (float)touch->yres) * FlxG::height);
		}

		// finger up
		else if(event.type == SDL_FINGERUP) {
			SDL_Touch *touch = SDL_GetTouch(event.tfinger.touchId);
			FlxMouse::onTouchEnd(event.tfinger.fingerId, ((float)event.tfinger.x / (float)touch->xres) * FlxG::width,
				((float)event.tfinger.y / (float)touch->yres) * FlxG::height);
		}

		// key down
		else if(event.type == SDL_KEYDOWN) {
			if(event.key.keysym.sym == SDLK_AC_BACK) keysDown[FlxKey::Escape] = true;
		}

		// key up
		else if(event.type == SDL_KEYUP) {
			if(event.key.keysym.sym == SDLK_AC_BACK) keysDown[FlxKey::Escape] = false;
		}
    }
}

FlxVector SDL_Mobile_Backend::getMousePosition(int index) {
    return FlxVector(0, 0);
}

bool SDL_Mobile_Backend::getMouseButtonState(int button, int index) {
    return false;
}


void SDL_Mobile_Backend::showMouse(bool show) {
    // no action
}

void SDL_Mobile_Backend::drawImage(FlxBackendImage *image, float x, float y, const FlxVector& scale, float angle,
                             const FlxRect& source, int color, bool flipped, float alpha)
{
	if(!image) return;

	SDL_Image *img = (SDL_Image*) image;
	if(!img->texture) return;

	FlxVector s = scale;
	s.x *= ((float)screenWidth / FlxG::width);
	s.y *= ((float)screenHeight / FlxG::height);
	
	x *= ((float)screenWidth / FlxG::width);
	y *= ((float)screenHeight / FlxG::height);
	
	SDL_Rect srcRect = { (int)source.x, (int)source.y, source.width, source.height };
	SDL_Rect destRect = { (int)x, (int)y, int(source.width * s.x), int(source.height * s.y) };

	SDL_SetTextureColorMod(img->texture, COLOR_GET_R(color), COLOR_GET_G(color), COLOR_GET_B(color));
	SDL_SetTextureAlphaMod(img->texture, int(alpha * 255.f));
	SDL_RenderCopyEx(renderer, img->texture, &srcRect, &destRect, -FlxU::radToDegrees(angle), NULL, flipped ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}

FlxBaseText *SDL_Mobile_Backend::createText(const wchar_t *text, void *font, int size, int color, float alpha) {
	if(!font) return NULL;
	
	// convert 8-bit string to 16-bit string
	std::wstring str(text);
	Uint16 *wstr = new Uint16[str.size()];
	for(unsigned int i = 0; i < str.size(); i++) wstr[i] = (Uint16)str[i];
	wstr[str.size()] = 0;
	
	// create surface with text
	SDL_Color colour = { (unsigned char)COLOR_GET_R(color), (unsigned char)COLOR_GET_G(color), (unsigned char)COLOR_GET_B(color), 
		(unsigned char)(alpha * 255.f) };
	SDL_Surface *txtSurface = TTF_RenderUNICODE_Solid((TTF_Font*)font, wstr, colour);

	delete[] wstr;
	
	SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, txtSurface);
	SDL_FreeSurface(txtSurface);

	int w = 0, h = 0;
	SDL_QueryTexture(tex, NULL, NULL, &w, &h);
	
	FlxBaseText *data = new FlxBaseText();
	data->data = tex;
    data->font = font;
    data->size = size;
    data->color = color;
    data->alpha = alpha;
    data->text = text;
    data->bounds.x = (float)w;
    data->bounds.y = (float)h;

	return data;
}

void SDL_Mobile_Backend::destroyText(FlxBaseText *data) {

    if(data) {
		if(data->data) SDL_DestroyTexture((SDL_Texture*)data->data);
        delete data;
    }
}

void SDL_Mobile_Backend::drawText(FlxBaseText *text, float x, float y, const FlxVector& scale, float angle) {
	if(!text) return;

	SDL_Texture *tex = (SDL_Texture*) text->data;
	if(!tex) return;

	FlxVector s = scale;
	s.x *= ((float)screenWidth / FlxG::width);
	s.y *= ((float)screenHeight / FlxG::height);
	
	x *= ((float)screenWidth / FlxG::width);
	y *= ((float)screenHeight / FlxG::height);
	
	SDL_Rect srcRect = { 0, 0, int(text->bounds.x), int(text->bounds.y) };
	SDL_Rect destRect = { (int)x, (int)y, int(text->bounds.x * s.x), int(text->bounds.y * s.y) };

	SDL_RenderCopyEx(renderer, tex, &srcRect, &destRect, -FlxU::radToDegrees(angle), NULL, SDL_FLIP_NONE);
}

bool SDL_Mobile_Backend::isShadersSupported() {
    SDL_RendererInfo info;
	SDL_GetRendererInfo(renderer, &info);

	return !strcmp(info.name, "opengles2");
}

FlxBackendShader* SDL_Mobile_Backend::loadShader(const char *path) {
    SDL_Shader *shader = new SDL_Shader();
	
    // load shader data
    FlxBackendFile *file = openFile(path, "r", true);
    if(!file) return NULL;

    file->seek(0, SEEK_END);
    unsigned int size = file->tell();
    file->seek(0, SEEK_SET);

    char *buffer = new char[size + 1];
	memset(buffer, 0, size + 1);
	
    file->read(buffer, size);
    delete file;

    std::string shaderData(buffer, buffer + size - 1);
    delete[] buffer;
	
	
    // create shaders
    shader->shaderProgram  = glCreateProgram();
    shader->vertexShader   = glCreateShader(GL_VERTEX_SHADER);
    shader->fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

    // set shaders source
    static int vertexShaderSize = sizeof(DefaultVertexShader);
    const GLchar *vert = DefaultVertexShader;
    glShaderSource(shader->vertexShader, 1, &vert, &vertexShaderSize);

    const GLchar *fragmentShaderCode = shaderData.c_str();
    int framgentShaderSize = (int)shaderData.length();
    glShaderSource(shader->fragmentShader, 1, &fragmentShaderCode, &framgentShaderSize);

    // compile shaders
    glCompileShader(shader->vertexShader);
    glCompileShader(shader->fragmentShader);
    glAttachShader(shader->shaderProgram, shader->vertexShader);
    glAttachShader(shader->shaderProgram, shader->fragmentShader);
    glLinkProgram(shader->shaderProgram);

    int compiled = 0;
    glGetShaderiv(shader->fragmentShader, GL_COMPILE_STATUS, &compiled);
    if(!compiled) return NULL;

    shaders.push_back(shader);
    return shader;
}

void SDL_Mobile_Backend::drawShader(FlxBackendShader *s) {
	if(!s) return;

    // get current framebuffer
    glBindTexture(GL_TEXTURE_2D, framebuffer);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, FlxG::width, FlxG::height, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    SDL_Shader *shader = (SDL_Shader*) s;
    glUseProgram(shader->shaderProgram);

    // bind all textures
    int i = 0;
    for(std::map<std::string, GLint>::iterator it = shader->textures.begin(); it != shader->textures.end();
        it++)
    {
        int location = glGetUniformLocation(shader->shaderProgram, it->first.c_str());
        glUniform1i(location, i);

        glActiveTexture((GLenum)(GL_TEXTURE0 + i));
        glEnable(GL_TEXTURE_2D);

        if(it->second != -1) {
            glBindTexture(GL_TEXTURE_2D, it->second);
        }
        else {
            glBindTexture(GL_TEXTURE_2D, framebuffer);
        }

        i++;
    }

    // Draw effect
	static GLfloat vertices[8] = {
		-1, -1,
		1, -1,
		-1, 1,
		1, 1
	};
    static GLfloat texCoords[8] = {
	    0, 0,
		1, 0, 
		0, 1,
		1, 1
	};
	
	int positionLocation = glGetAttribLocation(shader->shaderProgram, "a_position");
	int texcoordLocation = glGetAttribLocation(shader->shaderProgram, "a_texCoord");
	
	glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, vertices);
	glEnableVertexAttribArray(positionLocation);
	glVertexAttribPointer(texcoordLocation, 2, GL_FLOAT, GL_FALSE, 0, texCoords);
	glEnableVertexAttribArray(texcoordLocation);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	
    // unbind shader
    for(int j = i; j >= 0; j--) {
        glActiveTexture((GLenum)(GL_TEXTURE0 + j));
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }

    glActiveTexture(GL_TEXTURE0);
    glUseProgram(0);
}

FlxBackendImage* SDL_Mobile_Backend::createImage(int width, int height, int color, float alpha) {
	SDL_Image *img = new SDL_Image();
	img->texture = SDL_CreateTexture(renderer, 0, SDL_TEXTUREACCESS_STATIC, width, height);
	img->width = width;
	img->height = height;

    return img;
}

FlxBackendImage *SDL_Mobile_Backend::loadImage(const char *path) {

    if(images.find(path) != images.end()) {
        return images[path];
    }

	SDL_Image *img = new SDL_Image();
	img->texture = IMG_LoadTexture(renderer, path);

	if(img->texture) {
		SDL_QueryTexture(img->texture, NULL, NULL, &img->width, &img->height);
		images[path] = img;
	}
	else {
		return NULL;
	}
	
    return img;
}

void *SDL_Mobile_Backend::loadFont(const char *path, int fontSize) {

    std::stringstream ss;
    ss << path << "__size_" << fontSize;

    if(fonts.find(ss.str()) != fonts.end()) {
        return fonts[ss.str()];
    }

	TTF_Font *font = TTF_OpenFont(path, fontSize);
	if(font) {
		fonts[ss.str()] = font;
	}

    return font;
}

void* SDL_Mobile_Backend::loadSound(const char *path) {

    if(sounds.find(path) != sounds.end()) {
        return sounds[path];
    }

	Mix_Chunk *buffer = Mix_LoadWAV(path);
	if(buffer) {
		sounds[path] = buffer;
	}

    return buffer;
}

FlxBackendMusic* SDL_Mobile_Backend::loadMusic(const char *path) {

	SDL_Music *m = new SDL_Music();
	m->buffer = Mix_LoadMUS(path);
	if(m->buffer == NULL) return NULL;
	
    return m;
}

FlxBackendSound* SDL_Mobile_Backend::playSound(void *buffer, float vol) {
	if(!buffer) return NULL;

	SDL_Sound *sound = new SDL_Sound();
	sound->buffer = (Mix_Chunk*) buffer;
	sound->channel = -1;

	sound->setVolume(vol);
	sound->play();

    return sound;
}

void SDL_Mobile_Backend::playMusic(FlxBackendMusic *buff, float vol) {
    if(!buff) return;

	SDL_Music *music = (SDL_Music*) buff;
	if(!music->buffer) return;

	music->setVolume(vol);
	music->play();
}

// android/iphone data saving
// on android it requires WRITE_EXTERNAL_STORAGE permission
FlxBackendFile* SDL_Mobile_Backend::openFile(const char *path, const char *mode, bool internal) {
    SDL_File *file = new SDL_File();
    return file->open(path, mode, internal) ? file : NULL;
}


// android/iphone network
bool SDL_Mobile_Backend::sendHttpRequest(FlxHttpRequest *req, FlxHttpResponse& resp) {

	IPaddress ip;
	TCPsocket socket;
	char *responseBuffer = NULL;

	// try to open connection
	if(SDLNet_ResolveHost(&ip, req->host.c_str(), req->port) < 0) {
		return false;
	}

	if(!(socket = SDLNet_TCP_Open(&ip))) {
		return false;
	}

	// add content-lenght and content-type
	if(req->header.find("Content-Length") == req->header.end() && req->postData.length() != 0) {
		req->header["Content-Length"] = FlxU::toString((int)req->postData.length());
	}
	if(req->header.find("Content-Type") == req->header.end() && req->postData.length() != 0) {
		req->header["Content-Type"] = "application/octet-stream";
	}

	// build request
	std::stringstream requestBuffer;

	if(req->method == FLX_HTTP_GET) requestBuffer << "GET ";
	else requestBuffer << "POST ";

	requestBuffer << req->resource + " HTTP/1.0\r\n";

	for(std::map<std::string, std::string>::const_iterator it = req->header.begin(); it != req->header.end(); it++) {
		requestBuffer << it->first << ": " << it->second << "\r\n";
	}

	requestBuffer << "\r\n";
	requestBuffer << req->postData;

	SDLNet_TCP_Send(socket, (void*)requestBuffer.str().data(), requestBuffer.str().size());


	// get response
	static const unsigned int recvBufferSize = 8192;
	
	responseBuffer = new char[recvBufferSize];
	for(unsigned int i = 0; i < recvBufferSize; i++) responseBuffer[i] = 0;

	int received = SDLNet_TCP_Recv(socket, responseBuffer, recvBufferSize);
	if(received <= 0) return false;

	std::stringstream ss(std::string(responseBuffer, received));
	delete[] responseBuffer;

	std::string version;
	ss >> version; // skip protocol version
	ss >> resp.code;
	ss.ignore(10000, '\n');

	std::string line;
    while(std::getline(ss, line) && (line.size() > 2)) {

        unsigned int pos = line.find(": ");
        if(pos != std::string::npos) {

            std::string name = line.substr(0, pos);
            std::string value = line.substr(pos + 2);

            if (!value.empty() && (*value.rbegin() == '\r'))
                value.erase(value.size() - 1);

            resp.header[name] = value;
        }
    }

    std::copy(std::istreambuf_iterator<char>(ss), std::istreambuf_iterator<char>(), std::back_inserter(resp.data));

	SDLNet_TCP_Close(socket);
	return true;
}

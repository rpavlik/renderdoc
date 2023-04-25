/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include <algorithm>
#include "api/replay/version.h"
#include "strings/string_utils.h"
#include "xr_core.h"
// #include "vk_replay.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>


// embedded data file

extern unsigned char driver_openxr_renderdoc_json[];
extern int driver_openxr_renderdoc_json_len;

#if ENABLED(RDOC_ANDROID)
bool OpenXRReplay::CheckOpenXRLayer(OpenXRLayerFlags &flags, rdcarray<rdcstr> &myJSONs,
                                    rdcarray<rdcstr> &otherJSONs)
{
  return false;
}

void OpenXRReplay::InstallOpenXRLayer(bool systemLevel)
{
}
#else
static rdcstr GenerateJSON(const rdcstr &sopath)
{
  char *txt = (char *)driver_openxr_renderdoc_json;
  int len = driver_openxr_renderdoc_json_len;

  rdcstr json = rdcstr(txt, len);

  const char modulePathString[] = "@OPENXR_LAYER_MODULE_PATH@";

  int32_t idx = json.find(modulePathString);

  json = json.substr(0, idx) + sopath + json.substr(idx + sizeof(modulePathString) - 1);

  const char majorString[] = "@RENDERDOC_VERSION_MAJOR@";

  idx = json.find(majorString);
  while(idx >= 0)
  {
    json = json.substr(0, idx) + STRINGIZE(RENDERDOC_VERSION_MAJOR) +
           json.substr(idx + sizeof(majorString) - 1);

    idx = json.find(majorString);
  }

  const char minorString[] = "@RENDERDOC_VERSION_MINOR@";

  idx = json.find(minorString);
  while(idx >= 0)
  {
    json = json.substr(0, idx) + STRINGIZE(RENDERDOC_VERSION_MINOR) +
           json.substr(idx + sizeof(minorString) - 1);

    idx = json.find(minorString);
  }

  const char enableVarString[] = "@OPENXR_ENABLE_VAR@";

  idx = json.find(enableVarString);
  while(idx >= 0)
  {
    json = json.substr(0, idx) + STRINGIZE(ENABLE_OPENXR_RENDERDOC_CAPTURE) +
           json.substr(idx + sizeof(enableVarString) - 1);

    idx = json.find(enableVarString);
  }

  return json;
}

static bool FileExists(const rdcstr &path)
{
  return access(path.c_str(), F_OK) == 0;
}

static rdcstr GetSOFromJSON(const rdcstr &json)
{
  char *json_string = new char[1024];
  memset(json_string, 0, 1024);

  FILE *f = FileIO::fopen(json, FileIO::ReadText);

  if(f)
  {
    FileIO::fread(json_string, 1, 1024, f);

    FileIO::fclose(f);
  }

  rdcstr ret = "";

  // The line is:
  // "library_path": "/foo/bar/librenderdoc.so",
  char *c = strstr(json_string, "library_path");

  if(c)
  {
    c += sizeof("library_path\": \"") - 1;

    char *quote = strchr(c, '"');

    if(quote)
    {
      *quote = 0;
      ret = c;
    }
  }

  delete[] json_string;

  // get the realpath, if this is a real filename
  char *resolved = realpath(ret.c_str(), NULL);
  if(resolved && resolved[0])
  {
    ret = resolved;
    free(resolved);
  }

  return ret;
}

enum class LayerPath : int
{
  usr,
  First = usr,
  etc,
  home,
  Count,
};

ITERABLE_OPERATORS(LayerPath);

rdcstr LayerRegistrationPath(LayerPath path)
{
  switch(path)
  {
    case LayerPath::usr:
      return "/usr/share/openxr/implicit_layer.d/renderdoc_capture" STRINGIZE(
          RENDERDOC_OPENXR_JSON_SUFFIX) ".json";
    case LayerPath::etc:
      return "/etc/openxr/implicit_layer.d/renderdoc_capture" STRINGIZE(
          RENDERDOC_OPENXR_JSON_SUFFIX) ".json";
    case LayerPath::home:
    {
      const char *xdg = getenv("XDG_DATA_HOME");
      if(xdg && FileIO::exists(xdg))
        return rdcstr(xdg) + "/openxr/implicit_layer.d/renderdoc_capture" STRINGIZE(
                                 RENDERDOC_OPENXR_JSON_SUFFIX) ".json";

      const char *home_path = getenv("HOME");
      return rdcstr(home_path != NULL ? home_path : "") +
             "/.local/share/openxr/implicit_layer.d/renderdoc_capture" STRINGIZE(
                 RENDERDOC_OPENXR_JSON_SUFFIX) ".json";
    }
    default: break;
  }

  return "";
}

void MakeParentDirs(rdcstr file)
{
  rdcstr dir = get_dirname(file);

  if(dir == "/" || dir.empty())
    return;

  MakeParentDirs(dir);

  if(FileExists(dir))
    return;

  mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

bool OpenXRReplay::CheckOpenXRLayer(OpenXRLayerFlags &flags, rdcarray<rdcstr> &myJSONs,
                                    rdcarray<rdcstr> &otherJSONs)
{
  ////////////////////////////////////////////////////////////////////////////////////////
  // check that there's only one layer registered, and it points to the same .so file that
  // we are running with in this instance of renderdoccmd

  rdcstr librenderdoc_path;
  FileIO::GetLibraryFilename(librenderdoc_path);

  char *resolved = realpath(librenderdoc_path.c_str(), NULL);
  if(resolved && resolved[0])
  {
    librenderdoc_path = resolved;
    free(resolved);
  }

  if(librenderdoc_path.empty() || !FileExists(librenderdoc_path))
  {
    RDCERR("Couldn't determine current library path!");
    flags = OpenXRLayerFlags::ThisInstallRegistered;
    return false;
  }

  // it's impractical to determine whether the currently running RenderDoc build is just a loose
  // extract of a tarball or a distribution that decided to put all the files in the same folder,
  // and whether or not the library is in ld's searchpath.
  //
  // Instead we just make the requirement that renderdoc.json will always contain an absolute path
  // to the matching librenderdoc.so, so that we can check if it points to this build or another
  // build etc.
  //
  // Note there are three places to register layers - /usr, /etc and /home. The first is reserved
  // for distribution packages, so if it conflicts or needs to be deleted for this install to run,
  // we can't do that and have to just prompt the user. /etc we can mess with since that's for
  // non-distribution packages, but it will need root permissions.

  bool exist[arraydim<LayerPath>()];
  bool match[arraydim<LayerPath>()];

  int numExist = 0;
  int numMatch = 0;

  for(LayerPath i : values<LayerPath>())
  {
    exist[(int)i] = FileExists(LayerRegistrationPath(i));
    match[(int)i] = (GetSOFromJSON(LayerRegistrationPath(i)) == librenderdoc_path);

    if(exist[(int)i])
      numExist++;

    if(match[(int)i])
      numMatch++;
  }

  flags = OpenXRLayerFlags::UserRegisterable | OpenXRLayerFlags::UpdateAllowed;

  if(numMatch >= 1)
    flags |= OpenXRLayerFlags::ThisInstallRegistered;

  // if we only have one registration, check that it points to us. If so, we're good
  if(numExist == 1 && numMatch == 1)
    return false;

  if(numMatch == 1 && exist[(int)LayerPath::etc] && match[(int)LayerPath::etc])
  {
    // if only /etc is registered matching us, keep things simple and don't allow unregistering it
    // and registering the /home. Just unregister the /home that doesn't match
    flags &= ~(OpenXRLayerFlags::UserRegisterable | OpenXRLayerFlags::UpdateAllowed);
  }

  if(exist[(int)LayerPath::usr] && !match[(int)LayerPath::usr])
    otherJSONs.push_back(LayerRegistrationPath(LayerPath::usr));

  if(exist[(int)LayerPath::etc] && !match[(int)LayerPath::etc])
  {
    // if the /etc manifest doesn't match we need to elevate to fix it regardless of whether we
    // delete it in favour of a /home manifest, or if we update it.
    flags |= OpenXRLayerFlags::NeedElevation;
    otherJSONs.push_back(LayerRegistrationPath(LayerPath::etc));
  }

  if(exist[(int)LayerPath::home] && !match[(int)LayerPath::home])
    otherJSONs.push_back(LayerRegistrationPath(LayerPath::home));

  if(!otherJSONs.empty())
    flags |= OpenXRLayerFlags::OtherInstallsRegistered;

  if(exist[(int)LayerPath::usr] && match[(int)LayerPath::usr])
  {
    // just need to unregister others, but we can't user-local register anymore (as that would
    // require removing the one in /usr which we can't do)
    flags &= ~OpenXRLayerFlags::UserRegisterable;

    // any other manifests that exist, even if they match, are considered others.
    if(exist[(int)LayerPath::home])
    {
      otherJSONs.push_back(LayerRegistrationPath(LayerPath::home));
      flags |= OpenXRLayerFlags::OtherInstallsRegistered;
    }

    // any other manifests that exist, even if they match, are considered others.
    if(exist[(int)LayerPath::etc])
    {
      otherJSONs.push_back(LayerRegistrationPath(LayerPath::etc));
      flags |= OpenXRLayerFlags::OtherInstallsRegistered | OpenXRLayerFlags::NeedElevation;
    }
  }
  else
  {
    // if we have multiple matches but they are all correct, and there are no other JSONs we just
    // report that home needs to be unregistered.
    if(otherJSONs.empty() && exist[(int)LayerPath::etc] && match[(int)LayerPath::etc])
    {
      flags &= ~(OpenXRLayerFlags::UserRegisterable | OpenXRLayerFlags::UpdateAllowed);
      flags |= OpenXRLayerFlags::OtherInstallsRegistered;
      myJSONs.push_back(LayerRegistrationPath(LayerPath::etc));
      otherJSONs.push_back(LayerRegistrationPath(LayerPath::home));
    }
    else
    {
      myJSONs.push_back(LayerRegistrationPath(LayerPath::etc));
      myJSONs.push_back(LayerRegistrationPath(LayerPath::home));
    }
  }

  if(exist[(int)LayerPath::usr] && !match[(int)LayerPath::usr])
  {
    flags = OpenXRLayerFlags::Unfixable | OpenXRLayerFlags::OtherInstallsRegistered;
    otherJSONs.clear();
    otherJSONs.push_back(LayerRegistrationPath(LayerPath::usr));
  }

  return true;
}

void OpenXRReplay::InstallOpenXRLayer(bool systemLevel)
{
  rdcstr usrPath = LayerRegistrationPath(LayerPath::usr);
  rdcstr homePath = LayerRegistrationPath(LayerPath::home);
  rdcstr etcPath = LayerRegistrationPath(LayerPath::etc);

  if(FileExists(usrPath))
  {
    // if the usr path exists, all we can do is try to remove etc & home. This assumes a
    // system-level install
    if(!systemLevel)
    {
      RDCERR("Can't register user-local with manifest under /usr");
      return;
    }

    if(FileExists(homePath))
    {
      if(unlink(homePath.c_str()) < 0)
      {
        const char *const errtext = strerror(errno);
        RDCERR("Error removing %s: %s", homePath.c_str(), errtext);
      }
    }
    if(FileExists(etcPath))
    {
      if(unlink(etcPath.c_str()) < 0)
      {
        const char *const errtext = strerror(errno);
        RDCERR("Error removing %s: %s", etcPath.c_str(), errtext);
      }
    }

    return;
  }

  // if we want to install to the system and there's a registration in $HOME, delete it
  if(systemLevel && FileExists(homePath))
  {
    if(unlink(homePath.c_str()) < 0)
    {
      const char *const errtext = strerror(errno);
      RDCERR("Error removing %s: %s", homePath.c_str(), errtext);
    }
  }

  // and vice-versa
  if(!systemLevel && FileExists(etcPath))
  {
    if(unlink(etcPath.c_str()) < 0)
    {
      const char *const errtext = strerror(errno);
      RDCERR("Error removing %s: %s", etcPath.c_str(), errtext);
    }
  }

  LayerPath idx = systemLevel ? LayerPath::etc : LayerPath::home;

  rdcstr jsonPath = LayerRegistrationPath(idx);
  rdcstr path = GetSOFromJSON(jsonPath);
  rdcstr libPath;
  FileIO::GetLibraryFilename(libPath);

  if(path != libPath)
  {
    MakeParentDirs(jsonPath);

    FILE *f = FileIO::fopen(jsonPath, FileIO::WriteText);

    if(f)
    {
      fputs(GenerateJSON(libPath).c_str(), f);

      FileIO::fclose(f);
    }
    else
    {
      const char *const errtext = strerror(errno);
      RDCERR("Error writing %s: %s", jsonPath.c_str(), errtext);
    }
  }
}
#endif

package com.fulldivegames.majorasmaskvr;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.security.MessageDigest;
import java.util.*;
import org.json.*;

/** A persisted grant for MMVR only. No broad storage permissions are requested. */
final class SharedStorage {
 static final int REQUEST=47;
 static final String AUTHORITY="com.android.externalstorage.documents", ID="primary:MMVR";
 static final String README="Majora's Mask VR / 2Ship2Harkinian shared files\n\n"
  +"mods and texturepacks: unpack the downloaded ZIP; place .o2r or .otr packs here. Subfolders work.\n"
  +"VR Settings > System > Mods and texture packs: select this MMVR folder once. Restart to reload packs.\n"
  +"Packs load in alphabetical relative-path order; later packs win conflicts. Saved state selections preserve their order.\n"
  +"Use the VR menu checkboxes to enable or disable packs, then restart. VR item icons use pack textures.\n"
  +"cache/shaders is reserved; this preview does not persist compiled shader binaries there.\n"
  +"exports is for your manual backups. Active saves, settings and the imported game archive stay in\n"
  +"Android/data/com.fulldivegames.majorasmaskvr/files. Keep your own backup before uninstalling.\n"
  +"Android's folder picker grants access to MMVR only. Packs are cached privately for the native loader.\n";
 static void pick(Activity a){
  Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_WRITE_URI_PERMISSION|Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION|Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
  intent.putExtra(DocumentsContract.EXTRA_INITIAL_URI,DocumentsContract.buildDocumentUri(AUTHORITY,ID));
  a.startActivityForResult(intent,REQUEST);
 }
 static boolean allowed(String id){return ID.equals(id)||id.startsWith(ID+"/mods/")||id.equals(ID+"/mods")||id.equals(ID+"/texturepacks")||id.startsWith(ID+"/texturepacks/");}
 static synchronized void accept(Context c,Intent data)throws Exception{
  Uri tree=data.getData();
  if(tree==null||!AUTHORITY.equals(tree.getAuthority())||!allowed(DocumentsContract.getTreeDocumentId(tree)))throw new IOException("Select the MMVR folder at the root of headset storage. Create it in the picker if needed.");
  int flags=data.getFlags()&(Intent.FLAG_GRANT_READ_URI_PERMISSION|Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
  if((flags&3)!=3)throw new IOException("Read and write access to MMVR is needed.");
  c.getContentResolver().takePersistableUriPermission(tree,flags);
  c.getSharedPreferences("mmvr-shared-files",0).edit().putString("tree",tree.toString()).apply();
  if(ID.equals(DocumentsContract.getTreeDocumentId(tree)))layout(c,tree);
 }
 static class Entry {String id,name,mime;long size,modified;Entry(Cursor c){id=c.getString(0);name=c.getString(1);mime=c.getString(2);size=c.isNull(3)?-1:c.getLong(3);modified=c.isNull(4)?0:c.getLong(4);}}
 static List<Entry> children(Context c,Uri tree,String parent)throws Exception{
  List<Entry> result=new ArrayList<>();Uri query=DocumentsContract.buildChildDocumentsUriUsingTree(tree,parent);
  String[] cols={DocumentsContract.Document.COLUMN_DOCUMENT_ID,DocumentsContract.Document.COLUMN_DISPLAY_NAME,DocumentsContract.Document.COLUMN_MIME_TYPE,DocumentsContract.Document.COLUMN_SIZE,DocumentsContract.Document.COLUMN_LAST_MODIFIED};
  try(Cursor cursor=c.getContentResolver().query(query,cols,null,null,null)){
   if(cursor==null)throw new IOException("MMVR folder is unavailable");while(cursor.moveToNext()){
    Entry e=new Entry(cursor);if(e.id.startsWith(ID+"/")&&e.name!=null&&!e.name.contains("/")&&!e.name.equals(".."))result.add(e);
   }
  }
  result.sort(Comparator.comparing((Entry e)->e.name.toLowerCase(Locale.ROOT)).thenComparing(e->e.name));return result;
 }
 static Uri ensure(Context c,Uri tree,String parent,String name,String mime)throws Exception{
  for(Entry e:children(c,tree,parent))if(name.equals(e.name)){if(!mime.equals(e.mime)&&DocumentsContract.Document.MIME_TYPE_DIR.equals(mime))throw new IOException(name+" must be a folder");return DocumentsContract.buildDocumentUriUsingTree(tree,e.id);}
  Uri result=DocumentsContract.createDocument(c.getContentResolver(),DocumentsContract.buildDocumentUriUsingTree(tree,parent),mime,name);
  if(result==null)throw new IOException("Cannot create "+name);return result;
 }
 static void layout(Context c,Uri tree)throws Exception{
  for(String name:new String[]{"mods","texturepacks","exports"})ensure(c,tree,ID,name,DocumentsContract.Document.MIME_TYPE_DIR);
  Uri cache=ensure(c,tree,ID,"cache",DocumentsContract.Document.MIME_TYPE_DIR);ensure(c,tree,DocumentsContract.getDocumentId(cache),"shaders",DocumentsContract.Document.MIME_TYPE_DIR);
  Uri readme=ensure(c,tree,ID,"README.txt","text/plain");try(OutputStream out=c.getContentResolver().openOutputStream(readme,"wt")){if(out==null)throw new IOException("Cannot write MMVR README");out.write(README.getBytes(StandardCharsets.UTF_8));}
 }
 static void scan(Context c,Uri tree,String dir,String relative,File cache,JSONObject previous,JSONArray current,int depth,Set<String> visited,boolean force,List<String> warnings)throws Exception{
  if(depth>12||!visited.add(dir))throw new IOException("Pack folder nesting is too deep or repeated");
  final List<Entry> entries;
  try { entries=children(c,tree,dir); }
  catch(Exception failure) {
   warnings.add(relative+": "+failure.getMessage());
   for(Iterator<String> keys=previous.keys();keys.hasNext();) {
    JSONObject old=previous.getJSONObject(keys.next());
    if(old.optString("name").startsWith(relative)&&cached(cache,old))current.put(old);
   }
   return;
  }
  for(Entry e:entries){
   String name=relative+e.name;
   // The root grant also accepts user-created pack folders, but never imports
   // exported backups or reserved caches back into the running mod collection.
   if(relative.isEmpty()&&(e.name.equals("exports")||e.name.equals("cache")))continue;
   if(DocumentsContract.Document.MIME_TYPE_DIR.equals(e.mime)){
    try{scan(c,tree,e.id,name+"/",cache,previous,current,depth+1,visited,force,warnings);}
    catch(Exception failure){warnings.add(name+": "+failure.getMessage());}
    continue;
   }
   String lower=e.name.toLowerCase(Locale.ROOT);if(!lower.endsWith(".o2r")&&!lower.endsWith(".otr"))continue;
   if(relative.isEmpty()&&(lower.equals("mm.o2r")||lower.equals("2ship.o2r")))continue;
   JSONObject old=previous.optJSONObject(name);
   try {
    String ext=lower.endsWith(".o2r")?".o2r":".otr";
    // Retain legacy IDs so disabled choices from older releases migrate.
    String legacy=PackCache.hex(MessageDigest.getInstance("SHA-256").digest(e.id.getBytes(StandardCharsets.UTF_8)))+ext;
    Uri source=DocumentsContract.buildDocumentUriUsingTree(tree,e.id);
    String file=old==null?"":old.optString("file");
    String sha=old==null?"":old.optString("sha256");
    long size=old==null?-1:old.optLong("size",-1);
    boolean unchanged=cached(cache,old)&&e.modified>0&&old.optLong("modified")==e.modified&&size==e.size;
    if(force||!unchanged) {
     // Explicit refresh verifies source bytes even if a provider repeats its
     // timestamp. An unchanged huge pack requires no second full disk copy.
     PackCache.Result observed=null;
     if(cached(cache,old))observed=PackCache.hash(c.getContentResolver().openInputStream(source),e.size);
     // Older manifests used a path hash and did not record a content digest.
     // Verify that complete copy once rather than requiring another pack-sized
     // allocation just to migrate its metadata. Its file is never overwritten.
     if(observed!=null&&sha.isEmpty())
      sha=PackCache.hash(new FileInputStream(new File(cache,file)),size).sha256;
     if(observed!=null&&!sha.isEmpty()&&sha.equals(observed.sha256)) {
      size=observed.size;
     } else {
      PackCache.Result copied=PackCache.copy(c.getContentResolver().openInputStream(source),cache,ext,e.size);
      if(observed!=null&&!observed.sha256.equals(copied.sha256))throw new IOException("Pack changed during refresh; retry");
      file=copied.file;sha=copied.sha256;size=copied.size;
     }
    }
    current.put(new JSONObject().put("file",file).put("name",name).put("legacyFile",old==null?legacy:old.optString("legacyFile",legacy))
      .put("sha256",sha).put("size",size).put("modified",e.modified));
   } catch(Exception failure) {
    warnings.add(name+": "+failure.getMessage());
    android.util.Log.w("MMVR-Storage","Pack import failed: "+name,failure);
    // One broken/oversized archive must not suppress unrelated working packs.
    // Keep its previous complete copy when available; never index partial data.
    if(cached(cache,old))current.put(old);
   }
  }
 }
 static boolean cached(File cache,JSONObject entry){
  if(entry==null||!PackCache.validName(entry.optString("file")))return false;
  File file=new File(cache,entry.optString("file"));
  long size=entry.optLong("size",-1);
  return file.isFile()&&(size<0||file.length()==size);
 }
 static volatile String lastSyncStatus="";
 static String syncStatus(){return lastSyncStatus;}

 // Refresh and picker callbacks run on separate workers. One transaction owns
 // the shared staging filenames and manifest until its atomic publish finishes.
 static synchronized void sync(Context c)throws Exception{sync(c,false);}
 // An explicit refresh rereads pack contents even when a document provider
 // reports an unchanged or coarse modification time and identical file size.
 static synchronized void sync(Context c,boolean force)throws Exception{
  String selected=c.getSharedPreferences("mmvr-shared-files",0).getString("tree","");if(selected.isEmpty())return;
  Uri tree=Uri.parse(selected);if(!AUTHORITY.equals(tree.getAuthority())||!allowed(DocumentsContract.getTreeDocumentId(tree)))throw new IOException("Invalid shared folder selection");
  if(ID.equals(DocumentsContract.getTreeDocumentId(tree)))layout(c,tree);File root=c.getExternalFilesDir(null);if(root==null)throw new IOException("App storage is unavailable");File cache=new File(root,"shared-pack-cache");if(!cache.isDirectory()&&!cache.mkdirs())throw new IOException("Cannot prepare pack cache");
  File manifest=new File(cache,"packs.json");JSONObject previous=new JSONObject();
  if(manifest.isFile())try{JSONArray old=new JSONArray(SetupActivity.read(new FileInputStream(manifest)));for(int i=0;i<old.length();++i){JSONObject entry=old.getJSONObject(i);previous.put(entry.getString("name"),entry);}}catch(JSONException invalid){android.util.Log.w("MMVR-Storage","Rebuilding pack cache index");}
  JSONArray current=new JSONArray();Set<String> visited=new HashSet<>();List<String> warnings=new ArrayList<>();
  String selectedId=DocumentsContract.getTreeDocumentId(tree);
  if(ID.equals(selectedId))scan(c,tree,ID,"",cache,previous,current,0,visited,force,warnings);
  else scan(c,tree,selectedId,selectedId.substring(ID.length()+1)+"/",cache,previous,current,0,visited,force,warnings);
  File stage=new File(cache,"packs.json.pending");Files.write(stage.toPath(),current.toString(2).getBytes(StandardCharsets.UTF_8));Files.move(stage.toPath(),manifest.toPath(),StandardCopyOption.REPLACE_EXISTING,StandardCopyOption.ATOMIC_MOVE);
  lastSyncStatus="Prepared "+current.length()+" shared packs. Restart to apply selections.";
  if(!warnings.isEmpty())lastSyncStatus+=" "+warnings.size()+" import issue(s): "+warnings.get(0);
  android.util.Log.i("MMVR-Storage",lastSyncStatus);
 }
}

package com.fulldivegames.majorasmaskvr;
import org.libsdl.app.SDLActivity;
public final class GameActivity extends SDLActivity {
 private QuestUpdater updater;
 private SaveTransfer saveTransfer;
 private static java.lang.ref.WeakReference<GameActivity> active=new java.lang.ref.WeakReference<>(null);
 @Override protected void onCreate(android.os.Bundle saved){updater=new QuestUpdater(this);saveTransfer=new SaveTransfer(this);active=new java.lang.ref.WeakReference<>(this);super.onCreate(saved);}
 // Android's 2D window focus is not OpenXR session focus. Start SDL while the
 // Activity is resumed; OpenXR FOCUSED/STOPPING still gates gameplay and actions.
 @Override public void onWindowFocusChanged(boolean hasFocus){super.onWindowFocusChanged(true);}
 // The native engine has process-lifetime registries and cannot run main twice.
 // SDL first joins its game thread (including save/resource cleanup), then a full
 // activity close exits this app process. Headset sleep/onPause never takes this path.
 @Override protected void onDestroy(){
  boolean finished=isFinishing()&&!isChangingConfigurations();
  super.onDestroy();
  if(finished){android.util.Log.i("MMVR-Lifecycle","Native game closed; ending process for a clean relaunch");android.os.Process.killProcess(android.os.Process.myPid());}
 }
 @Override protected void onResume(){super.onResume();if(updater!=null)updater.resumed();}
 private final java.util.concurrent.atomic.AtomicBoolean packsRefreshing=new java.util.concurrent.atomic.AtomicBoolean();
 public void refreshMMVRPacks(){
  if(!packsRefreshing.compareAndSet(false,true))return;
  updater.report("Refreshing packs...");
  new Thread(()->{try{
   if(getSharedPreferences("mmvr-shared-files",0).getString("tree","").isEmpty())
    updater.report("Connect the MMVR shared folder once, then refresh. Unpack ZIPs into mods or texturepacks.");
   else {SharedStorage.sync(this,true);updater.report(SharedStorage.syncStatus());}
  }catch(Exception e){updater.report("Pack refresh: "+e.getMessage());}finally{packsRefreshing.set(false);}},"MMVR-packs").start();
 }
 public void requestMMVRSharedFolder(){runOnUiThread(()->{try{SharedStorage.pick(this);}catch(Exception e){updater.report("Folder picker unavailable: "+e.getMessage());}});}
 @Override protected void onActivityResult(int request,int result,android.content.Intent data){super.onActivityResult(request,result,data);
  if(request==SaveTransfer.REQUEST){saveTransfer.selected(result,data);return;}
  if(request!=SharedStorage.REQUEST||result!=RESULT_OK||data==null)return;
  new Thread(()->{try{SharedStorage.accept(this,data);SharedStorage.sync(this,true);updater.report(SharedStorage.syncStatus());}catch(Exception e){updater.report("Shared folder: "+e.getMessage());}},"MMVR-folder").start();
 }
 public void requestMMVRUpdate(boolean install){updater.request(install);}
 public void requestMMVRSaveTransfer(boolean exporting){saveTransfer.request(exporting);}
 public String takeMMVRSaveTransferResult(){return saveTransfer.takeResult();}
 public String getMMVRUpdateStatus(){return updater.status();}
 private final java.util.concurrent.atomic.AtomicBoolean stateRestarting=new java.util.concurrent.atomic.AtomicBoolean();
 // Called on SDL's native game thread. Wait only for a bounded ready handshake;
 // never wait for Activity destruction, which must join that same game thread.
 public String requestMMVRStateRestart(){
  if(android.os.Looper.myLooper()==android.os.Looper.getMainLooper())return "Restart must be requested from the game thread.";
  if(!stateRestarting.compareAndSet(false,true))return "A restart is already in progress.";
  java.util.concurrent.CountDownLatch ready=new java.util.concurrent.CountDownLatch(1);
  java.util.concurrent.atomic.AtomicInteger phase=new java.util.concurrent.atomic.AtomicInteger(0);
  java.util.concurrent.atomic.AtomicReference<android.os.ResultReceiver> confirmation=new java.util.concurrent.atomic.AtomicReference<>();
  java.util.concurrent.atomic.AtomicReference<String> failure=new java.util.concurrent.atomic.AtomicReference<>("");
  // This process-owned Binder becomes dead only after SDL's normal shutdown and
  // our onDestroy cleanup. The separate restart Activity cannot kill this app.
  android.os.Binder owner=new android.os.Binder();
  android.os.ResultReceiver receiver=new android.os.ResultReceiver(new android.os.Handler(android.os.Looper.getMainLooper())){
   @Override protected void onReceiveResult(int code,android.os.Bundle data){
    android.os.ResultReceiver commit=data==null?null:data.getParcelable(StateRestartActivity.COMMIT);
    if(code!=RESULT_OK||commit==null){failure.set("Android restart helper could not prepare.");ready.countDown();return;}
    confirmation.set(commit);
    if(phase.get()==2)commit.send(RESULT_CANCELED,null);
    ready.countDown();
   }
  };
  runOnUiThread(()->{try{
   if(phase.get()!=0)return;
   android.os.Bundle request=new android.os.Bundle();
   request.putBinder(StateRestartActivity.OWNER,owner);
   request.putParcelable(StateRestartActivity.READY,receiver);
   startActivity(new android.content.Intent(this,StateRestartActivity.class).putExtras(request));
  }catch(RuntimeException error){failure.set("Android could not open the restart helper.");ready.countDown();}});
  try{
   if(!ready.await(5,java.util.concurrent.TimeUnit.SECONDS))failure.set("Android restart helper timed out.");
   android.os.ResultReceiver commit=confirmation.get();
   if(failure.get().isEmpty()&&commit!=null){
    phase.set(1);commit.send(RESULT_OK,null);
    // Normal Activity finish sends SDL_QUIT and releases its pause semaphore,
    // then joins the native thread. This also handles a pause caused by opening
    // the helper panel; no kill occurs until SDL has completed its cleanup.
    runOnUiThread(this::finish);
    return "";
   }
   if(failure.get().isEmpty())failure.set("Android restart helper is unavailable.");
  }catch(InterruptedException interrupted){Thread.currentThread().interrupt();failure.set("Android restart was interrupted.");}
  phase.set(2);
  android.os.ResultReceiver commit=confirmation.get();if(commit!=null)commit.send(RESULT_CANCELED,null);
  stateRestarting.set(false);
  return failure.get();
 }
 static void updateResult(String text){GameActivity activity=active.get();if(activity!=null&&activity.updater!=null)activity.updater.installationResult(text);}

 @Override protected String[] getLibraries(){return new String[]{"c++_shared","SDL2","openxr_loader","2ship"};}
 @Override protected String getMainSharedObject(){return getApplicationInfo().nativeLibraryDir+"/lib2ship.so";}
}

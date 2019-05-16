using System.Collections;
using System.Collections.Generic;
using UnityEngine;


////////////////////////////////////////////
// Code Example Description

/*
* Just in Time loader that I have written for Super Animals 3 because we were running into some memory limitations on certain mobile devices.
* In hindsight I would not have used a JitUser abstract class to store additional properties but rather use a struct as a data container for the additional parameters.
* This way classes would not be restricted to JitUser inheritance and improve usage flexibility of the Jit loader.
*/

namespace Jit {
    // Callback when the texture is loaded
    public interface IJitUser {
        void ItemReady(Texture2D item);
    }

    public abstract class JitUser : MonoBehaviour, IJitUser {
        // Add additional properties to be accesesed from the JitLoader
        // We do this to prevent having to overload JitLoader functions
        public bool bLoadInstant = false;
        public abstract void ItemReady(Texture2D item);
    }

    enum ASyncDelay {
        EndOfFrame,
        Seconds
    }

    struct QueueData {
        public QueueData(string key, byte[] bytes, JitUser caller) {
            Key = key;
            Bytes = bytes;
            Caller = caller;
        }

        private string Key;
        private byte[] Bytes;
        private JitUser Caller;

        public string GetKey { get { return Key; } }
        public byte[] GetBytes { get { return Bytes; } }
        public JitUser GetCaller { get { return Caller; } }
    }

    /**
     * All classes using the Jitloader must inherit from JitUser or a similar class
     * JitUser provides a set of methods and fields that the JitLoader needs to operate
     **/
    public class JitLoader : MonoBehaviour {

        public static JitLoader Instance { private set; get; }

        [SerializeField] private FilterMode filterMode;

        [Tooltip("When do we start loading the next item")]
        [SerializeField] private ASyncDelay delayType;

        [Tooltip("The delay before the next load, only works with DelayType 'Seconds'")]
        [SerializeField] private float delayAmount;

        private Dictionary<string, Texture2D> loadedTextures = new Dictionary<string, Texture2D>();
        private Queue<QueueData> textureQueue = new Queue<QueueData>();
        private bool isProcessingQueue = false;

        private void Awake() {
            if (FindObjectsOfType<JitLoader>().Length > 1) {
                Destroy(this.gameObject);
                return;
            }
            DontDestroyOnLoad(this.gameObject);
            Instance = this;
        }

        public void GetCardGraphicByKey(string textureName, JitUser caller) {
            if (textureName == null) { return; }
            if (caller == null) { return; }

            // Check if the texture is already in the memory
            if (loadedTextures.ContainsKey(textureName)) {
                caller.ItemReady(loadedTextures[textureName]);
                return;
            }

            StartCoroutine(GetImageResource(textureName, caller));
        }

        private IEnumerator ProgressQueue() {
            if (isProcessingQueue) { yield break; }
            isProcessingQueue = true;

            while (textureQueue.Count > 0) {
                Texture2D texture;
                QueueData textureData = textureQueue.Dequeue();

                if (loadedTextures.ContainsKey(textureData.GetKey)) {
                    texture = loadedTextures[textureData.GetKey];
                } else {
#if UNITY_IOS
                    texture = new Texture2D(1, 1, TextureFormat.PVRTC_RGB4, false);
#else
                    texture = new Texture2D(1, 1, TextureFormat.RGBA32, false);
#endif
                    texture.filterMode = filterMode;
                    texture.LoadImage(textureData.GetBytes);

                    loadedTextures.Add(textureData.GetKey, texture);
                }

                textureData.GetCaller.ItemReady(texture);

                if (delayType == ASyncDelay.EndOfFrame) { yield return new WaitForEndOfFrame(); }
                if (delayType == ASyncDelay.Seconds) { yield return new WaitForSeconds(delayAmount); }
            }

            isProcessingQueue = false;
            yield break;
        }

        private IEnumerator GetImageResource(string textureName, JitUser caller) {
            byte[] bytes = null;
            string filePath = "";
            string extension = ".jpg";

#if UNITY_EDITOR
            filePath = Application.streamingAssetsPath + "/Cards/" + textureName + extension;
            if (System.IO.File.Exists(filePath)) { bytes = System.IO.File.ReadAllBytes(filePath); }
#elif UNITY_ANDROID
            filePath = "jar:file://" + Application.dataPath  + "!/assets/" + "Cards/" + textureName + extension;
            WWW www = new WWW(filePath);
            yield return www;
            bytes = www.bytes;

            if (!string.IsNullOrEmpty(www.error)) {
                Debug.LogError ("JitLoader.cs: GetImageResource() [UNITY_ANDROID]: Can't read file path!");
                Debug.Log("WWW.error: " + www.error);
            }
#elif UNITY_IOS
            filePath = "file://" + Application.dataPath + "/Raw" + "/Cards/" + textureName + extension;
            WWW www = new WWW(filePath);
            yield return www;
            bytes = www.bytes;

            if (!string.IsNullOrEmpty(www.error)) {
                Debug.LogError ("JitLoader.cs: GetImageResource() [UNITY_IOS]: Can't read file path!");
                Debug.Log("WWW.error: " + www.error);
            }
#endif
            if (caller.bLoadInstant) {
                ProcessImageResource(textureName, bytes, caller);
            } else {   
                ProcessASyncImageResource(textureName, bytes, caller);
            }

            yield return null;
        }

        private void ProcessASyncImageResource(string textureName, byte[] bytes, JitUser caller) {
            if (bytes == null) { return; }
            if (bytes.Length <= 0) { return; }

            QueueData data = new QueueData(textureName, bytes, caller);
            if (textureQueue.Contains(data)) { return; }
            textureQueue.Enqueue(data);

            if (!isProcessingQueue) {
                StartCoroutine(ProgressQueue());
            }
        }

        private void ProcessImageResource(string textureName, byte[] bytes, JitUser caller) {
            if (bytes == null) { return; }
            if (bytes.Length <= 0) { return; }

            Texture2D texture;

#if UNITY_IOS
            texture = new Texture2D(1, 1, TextureFormat.PVRTC_RGB4, false);
#else
            texture = new Texture2D(1, 1, TextureFormat.RGBA32, false);
#endif
            texture.filterMode = filterMode;
            texture.LoadImage(bytes);

            loadedTextures.Add(textureName, texture);
            caller.ItemReady(texture);
        }

        public void UnloadCardGraphicByKey(string textureName, JitUser caller) {
            if (textureName == null) { return; }
            if (caller == null) { return; }

            if (loadedTextures.ContainsKey(textureName)) {
                Destroy(loadedTextures[textureName]);
                loadedTextures.Remove(textureName);
            }
        }
    }
}
# Tsurido - M5StickCで投げ釣りの当たり判定をする

# DEMO

![センス](sense.png)

作成中...

# 機能

竿先に加速度センサー(ADXL345)を取り付けて魚のアタリを検知します。加速度の統計的外れ値を検出してアタリを判定します。


# 必須条件

センサー側：

* M5StickC
* ADXL345

クライアント側：

* Python3が動く環境
* Bluetoothアダプターが使えること

# インストール

以下のライブラリをインストールしてください。

* 必須ライブラリ
  - Adafruit_BluefruitLE
  - numpy
  - matplotlib
  - pyaudio  
  - simpleaudio
  - pydub

例えばubuntu20.04だと以下のような順番でインストール可能です。

Adafruit_BluefruitLEは公式リポジトリ版は最新のBluezに対応できてないためbluezのバージョンが以下のレポジトリのものをインストールします。

パッチ適用済み Adafruit Python BluefruitLE ライブラリのインストール

```bash
git clone　https://github.com/donatieng/Adafruit_Python_BluefruitLE
cd Adafruit_Python_BluefruitLE
sudo python3 setup.py install
```

pyaudioをインストールする前にportaudioをインストールする必要があります。

ubuntu系の場合
```bash
sudo apt-get install portaudio19-dev
```

残りのライブラリのインストール

```bash
sudo pip install numpy matplotlib pyaudio simpleaudio pydub
```

# 使い方

DEMOの実行方法など、"hoge"の基本的な使い方を説明する

```bash
git clone https://github.com/hotstaff/tsurido
cd tsurido
python client.py
```

# 注意

本プログラムは、あなたに対して何も保証しません。あなたが、本プログラムを利用（閲覧、投稿、外部での再利用など全てを含む）する場合は、自己責任で行う必要があります。

- 利用の結果生じた損害及び釣果について、一切責任を負いません。
- あなたの適用される法令に照らして、本プログラムの利用が合法であることを保証しません。
- コンテンツとして提供する全ての文章、画像、音声情報について、内容の合法性・正確性・安全性等、あらゆる点において保証しません。
- リンクをしている外部サイトについては、何ら保証しません。
- 事前の予告無く、コンテンツの提供を中止する可能性があります。

# 作者

* Hideto Manjo

# License

"tsurido" は [LGPL](./LICENCE)ライセンスの元で公開されています。
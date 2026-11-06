<p align="center">
  <a href="README.md">English</a> | <b>العربية</b>
</p>

<div dir="rtl">

# Lichii لـ The Duskbloods (النسخة التجريبية للشبكة)

**هذا المشروع نسخة معدّلة (fork) من [Frizti/Lichii](https://github.com/Frizti/Lichii) (`frizti-v2`)، ومحسّنة لتشغيل *The Duskbloods Network Test* (رقم البرنامج `0400D1D02399A000`) على أجهزة Windows بكرت شاشة NVIDIA ومعالج Intel.**
يعمل مع المشغّل [playDB-CNT](https://github.com/sikoGD/playDB-CNT) من sikoGD، والذي يوفّر خوادم الشبكة المحلية التي تحتاجها اللعبة للدخول.

> المشروع غير تابع لـ Nintendo أو FromSoftware أو المشاريع الأصلية. لا يتضمن أي ألعاب أو مفاتيح أو firmware. استخدم فقط ملفات استخرجتها بنفسك بطريقة قانونية.

## الأداء

قياسات صاحب المشروع أثناء اللعب الفعلي:

| | |
|---|---|
| المعالج | Intel Core i5-12400F |
| كرت الشاشة | NVIDIA GeForce RTX 3060 (12 GB)، تعريف 616.56 |
| الذاكرة | 16 GB |
| المتوسط | **حوالي 40 إطار/ثانية** |
| الأعلى | **حوالي 50 إطار/ثانية** |
| أثقل منطقة | **أكثر من 36 إطار/ثانية** (كانت 13-16 قبل تحسينات هذه النسخة) |

المحاكي في هذه اللعبة يعتمد على المعالج أكثر من كرت الشاشة، لذلك يفيد المعالج الأسرع أكثر من كرت شاشة أقوى.

## ما الذي تغيّر في هذه النسخة

| المجال | الإصلاح |
|---|---|
| فحص الاتصال | يعمل مع مشغّل playDB-CNT: إعادة توجيه الخوادم، شبكة محلية فقط، إضافة شهادة المشغّل إلى شهادات اللعبة الموثوقة، وإصلاح خطأ BCAT ‏(2122-0001) |
| الرسوميات | الخطوط السوداء (TAA)، الضباب الذي يتحوّل للأبيض، الموجات البيضاء أسفل الشاشة، وتعليمتان مفقودتان في shaders ‏Ampere |
| الأداء | بحث أسرع في نطاقات الذاكرة، تجاوز مزامنة البيانات غير المتغيّرة، وخيط GPU أمامي ينام بدل الانشغال بالانتظار |

**تمّت التجربة على:** كرت شاشة **NVIDIA RTX 3060** مع معالج **Intel Core i5-12400F**. يُفترض أن تعمل كروت NVIDIA الأخرى بنفس الطريقة، أما كروت AMD وIntel فلم تُجرَّب. استخدم أحدث تعريف لكرت الشاشة.

## التحميل

**[حمّل أحدث إصدار (Lichii-Duskbloods.zip)](https://github.com/Mr-Tb/lichii-dusk/releases/latest)**

## التثبيت

تحتاج إلى:
- Windows 10/11 بنظام 64-bit وكرت شاشة يدعم Vulkan 1.3 (يُنصح بـ NVIDIA)
- ملفّي `prod.keys` و`title.keys` الخاصين بك
- نسخة firmware خاصة بك من Switch 2 (تمّت التجربة على 23.0.1)
- نسختك من *The Duskbloods Network Test*
- **[مشغّل playDB-CNT](https://github.com/sikoGD/playDB-CNT)** ‏(`Play.Duskbloods.exe`)، مع اتباع الشرح في صفحته لإعداده

الخطوات:

1. **فكّ الضغط** عن `Lichii-Duskbloods.zip` في مجلد بمسار قصير، مثل `C:\Games\Lichii-Duskbloods\`. أبقِ مجلد `user` بجانب `lichii.exe`: فهو يجعل المحاكي محمولًا ويحتوي على الإعدادات المحسّنة.
2. **المفاتيح:** انسخ `prod.keys` و`title.keys` إلى `user\keys\`، أو شغّل `lichii.exe` واختر **Tools > Install Decryption Keys**.
3. **الـ Firmware:** شغّل `lichii.exe`، ثم اختر **Tools > Install Firmware > From Folder** (أو **From ZIP**) وحدّد ملفات الـ firmware. ثم أغلق المحاكي.
4. **المشغّل:** جهّز [playDB-CNT](https://github.com/sikoGD/playDB-CNT) كما في شرحه، ثم عدّل ملف `launcher_settings.json` بحيث يشير `emulator` إلى `lichii.exe`:

<div dir="ltr">

```json
{
  "emulator": "C:\\Games\\Lichii-Duskbloods\\lichii.exe",
  "game": "C:\\Games\\THE DUSKBLOODS NETWORK TEST VER"
}
```

</div>

   استخدم شرطتين مائلتين `\\`. القيمة `game` هي مجلد اللعبة.

5. **اللعب:** شغّل `Play.Duskbloods.exe` واضغط **Play**. شغّل اللعبة دائمًا من المشغّل، لأن تشغيلها مباشرة يُفشل فحص الاتصال.

في الدقائق الأولى قد يحدث تقطيع بسيط أثناء تجهيز الـ shaders، ويتحسّن ذلك مع كل جلسة.

## الإعدادات المقترحة (مضبوطة مسبقًا في الملف المضغوط)

| الإعداد | القيمة | السبب |
|---|---|---|
| Graphics > Advanced > GPU Mode | **Fast** | توفير كبير في استهلاك المعالج |
| Extras > Asynchronous shader compilation | **On** | تقطيع أقل |
| Graphics > VSync | **Mailbox** | العرض لا يعطّل الرسم |
| Async presentation | **Off** | أسرع في هذه اللعبة |
| Window adapting filter | Bilinear | الأخفّ |
| Error applet | HLE | نسخة LLE تُسقط المحاكي |
| Docked mode | On | |

## حل المشاكل

- **التوقف عند "checking save data" أو أخطاء الشبكة:** شغّل اللعبة من `Play.Duskbloods.exe` وليس من `lichii.exe` مباشرة.
- **الخطأ 2153-0513 عند التشغيل:** ملف الحفظ تلف، مثلًا بسبب إغلاق اللعبة بالقوة أثناء الحفظ. انقل مجلد حفظ اللعبة داخل `user\nand\user\save\` إلى مكان آخر وأعد التشغيل.
- **شاشة سوداء بعد التحميل:** أغلق اللعبة وأعد تشغيلها (حدث تعليق نادر مرة واحدة).
- **شاشة سوداء أول تشغيل بعد التحديث:** احذف ذاكرة الـ shaders في `user\shader\`.
- **رسائل عن ملفات DLL مفقودة:** فكّ الضغط عن الملف كاملًا، ولا تشغّل `lichii.exe` من داخل الملف المضغوط.

## البناء من الكود المصدري

الفرع [`dusk-frizti`](https://github.com/Mr-Tb/lichii-dusk/tree/dusk-frizti). يُبنى باستخدام MSYS2 UCRT64 + clang مثل Lichii الأصلي تمامًا.

## الشكر والترخيص

مبني على [Frizti/Lichii](https://github.com/Frizti/Lichii) و[zortos293/Lichii](https://github.com/zortos293/Lichii) و[Eglantine](https://github.com/Djihads80/Eglantine) وEden وyuzu وSudachi. المشغّل وخوادم الشبكة من [playDB-CNT](https://github.com/sikoGD/playDB-CNT) لـ sikoGD. الترخيص: GPL-3.0-or-later (راجع [LICENSE.txt](LICENSE.txt)).

</div>

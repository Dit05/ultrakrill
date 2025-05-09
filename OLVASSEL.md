# ultrakrill
Egyszerű végtelen futós játék Arduino UNO-ra egy karakter LCD-vel, melyet az [ULTRAKILL](https://en.wikipedia.org/wiki/Ultrakill) ihletett.

## Játékmenet
Automatikusan jobbra haladsz, és el kell kerülnöd az akadályokat és ellenségeket, hogy minél tovább juss. Az életerőd lassan csökken, és csak vér fogyasztásával nyerhető vissza. 9 bugyor van, mindegyik gyorsabb haladással és újabb mintákkal az előzőnél.

### Pályaelemek
- **Fal**: Limitált számú lövést nyel el, és megsebez ha belemész.
- **Bozóttúz**: Menőn néz ki, de az egyetlen célja, hogy téged sebezzen.
- **Piszok**: Az egyik fajta ellenség. Nem csinál sok mindent, de jó vérforrás.
- **Ördög**: A másik fajta ellenség. Néha Pokol Energia gömböket lő feléd.

Nyomd meg a *tűz* gombot a főmenüben a kezdéshez. Az intró felgyorsítható a *le*- és átugorható a *tűz* gombbal.

### Irányítás
Ugorj a *fel* gombbal, csússz a *le*-vel.
Nyomd meg röviden a *tűz* gombot a lövéshez, vagy tartsd lenyomva egy erősebb lövés feltöltéséhez.
Amikor egy ellenséges lövés épp előtted van, akkor a *tűz*-zel kiparryzhető, ezzel visszaküldve azt.

A játék vége képernyőn a *le* és *fel* segítségével megnézheted a statisztikáid. Ezeket elküldődnek serial-on keresztül is JSON-ként meghaláskor.

## Játékmenet tippek
- Ha ugrasz rögtön egy csúszás után, akkor gyorsan tudsz a levegőben haladni.
- A robbanós lövések több vért ontanak.
- Az ördögök több vért tartalmaznak a piszkoknál, de magasabb életerejük is van.
- A vér gyorsan elpárolog, tehát jó ötlet közelengedni magadhoz az ellenségeket.
- A falak tetején lehet járni, sőt, csúszni is.

## Hardware összeállítás
Hozzávalók:
- Arduino UNO R3
- Karakteres LCD (16x2 ajánlott, a játék szélessége beállítható az `LCD_WIDTH`-tel)
- Potméter az LCD kontraszt állítgatására (opcionális, de erősen ajánlott)
- 3 gomb

A pinek a forráskód teteje táján találhatók. A gombok INPUT_PULLUPként vannak használva, tehát nem kellenek pull-up ellenállások. Az LCD RW pinje nem használt, tehát ez a földhöz kötendő.

## Kompilálás
Az `ultrakill` mappa egy Arduino sketch, amely kompilálható és feltölthető az arduino-cli vagy egy hasonló eszköz segítségével.

Az emulátor amit a makefile gyárt régen egy működő program volt, ami SDL-lel kamuzta az Arduino funkciókat a programnak, számítógépen történő futtatás céljából, viszont ez a PROGMEM-es cuccok behozása óta nem lett frissítve.

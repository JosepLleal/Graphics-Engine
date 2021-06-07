# Graphics Engine - Advanced Tecniques
Deferred Shading tecnique implemented by two 4th year students: Victor Chen and Josep Lleal for the Advanced Graphics Programming subject.

## Camera controls
- W: Move forward
- S: Move backward
- D: Move right
- A: Move left
- Left Mouse Click + drag: Rotate camera

## Team
Josep Lleal
   - [Josep's GitHub Link](https://github.com/JosepLleal)
   
Víctor Chen
   - [Victor's GitHub Link](https://github.com/Scarzard)

## Render modes
### Forward shading

<p align="center">
  <img  src="https://raw.githubusercontent.com/JosepLleal/Graphics-Engine/main/images/FORWARD.PNG" width="1200">
</p>

### Deferred shading

<p align="center">
  <img  src="https://raw.githubusercontent.com/JosepLleal/Graphics-Engine/main/images/DEFERRED.PNG" width="1200">
</p>
   
## Implemented tecniques
### Normal and relief mapping
- Tecnique ON:

<p align="center">
  <img  src="https://raw.githubusercontent.com/JosepLleal/Graphics-Engine/main/images/RELIEF_ON.PNG" width="1200">
</p>

- Tecnique OFF:

<p align="center">
  <img  src="https://raw.githubusercontent.com/JosepLleal/Graphics-Engine/main/images/RELIEF_OFF.PNG" width="1200">
</p>

### SSAO (Screen Space Ambient Occlusion)

- Tecnique ON:

<p align="center">
  <img  src="https://raw.githubusercontent.com/JosepLleal/Graphics-Engine/main/images/SSAO_ON.PNG" width="1200">
</p>

- Tecnique OFF:

<p align="center">
  <img  src="https://raw.githubusercontent.com/JosepLleal/Graphics-Engine/main/images/SSAO_OFF.PNG" width="1200">
</p>

#### How to eanble/disable and configure the tecniques

<p align="center">
  <img  src="https://raw.githubusercontent.com/JosepLleal/Graphics-Engine/main/images/WIDGETS.PNG" width="500">
</p>

- To enable/disable relief mapping, you can check the box named "Relief Mapping" in the UI.
- To configure, you can set the bumpiness in the UI.

- To enable/disable SSAO, you can check the box named "SSAO" in the UI.
- To configure, you can set the bias and the radius in the UI.

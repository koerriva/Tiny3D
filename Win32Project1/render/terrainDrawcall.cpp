#include "terrainDrawcall.h"
#include "../render/render.h"

// Attribute slots
const uint VertexSlot = 0;
const uint NormalSlot = 1;
const uint TexcoordSlot = 2;
const uint TexidSlot = 3;
const uint ColorSlot = 4;
const uint TangentSlot = 5;

// VBO index
const uint VertexIndex = 0;
const uint NormalIndex = 1;
const uint TexcoordIndex = 2;
const uint TexidIndex = 3;
const uint ColorIndex = 4;
const uint TangentIndex = 5;
const uint OutIndex = 6;
const uint IndirectIndex = 7;

// SSBO index
const uint ChunkIndex = 0;
const uint InputIndex = 1;

TerrainDrawcall::TerrainDrawcall(Terrain* terrain, Batch* batch) {
	mesh = terrain;
	data = batch;
	vertexCount = data->vertexCount;
	maxIndexCount = data->indexCount;
	chunkCount = mesh->chunks.size();

	indirectBuffer = (Indirect*)malloc(sizeof(Indirect));
	indirectBuffer->count = 0;
	indirectBuffer->firstIndex = 0;
	indirectBuffer->primCount = 1;
	indirectBuffer->baseVertex = 0;
	indirectBuffer->baseInstance = 0;

	setType(TERRAIN_DC);
	dataBuffer = createBuffers();
	ssBuffer = createSSBuffers();
	data->releaseBatchData();
}

RenderBuffer* TerrainDrawcall::createBuffers() {
	RenderBuffer* buffer = new RenderBuffer(8);
	buffer->setAttribData(GL_ARRAY_BUFFER, VertexIndex, VertexSlot, GL_FLOAT, vertexCount, 3, 1, false, GL_STATIC_DRAW, 0, data->vertexBuffer);
	buffer->setAttribData(GL_ARRAY_BUFFER, NormalIndex, NormalSlot, GL_FLOAT, vertexCount, 3, 1, false, GL_STATIC_DRAW, 0, data->normalBuffer);
	buffer->setAttribData(GL_ARRAY_BUFFER, TexcoordIndex, TexcoordSlot, GL_FLOAT, vertexCount, 4, 1, false, GL_STATIC_DRAW, 0, data->texcoordBuffer);
	buffer->setAttribData(GL_ARRAY_BUFFER, TexidIndex, TexidSlot, GL_FLOAT, vertexCount, 4, 1, false, GL_STATIC_DRAW, 0, data->texidBuffer);
	buffer->setAttribData(GL_ARRAY_BUFFER, ColorIndex, ColorSlot, GL_UNSIGNED_BYTE, vertexCount, 3, 1, false, GL_STATIC_DRAW, 0, data->colorBuffer);
	buffer->setAttribData(GL_ARRAY_BUFFER, TangentIndex, TangentSlot, GL_FLOAT, vertexCount, 3, 1, false, GL_STATIC_DRAW, 0, data->tangentBuffer);
	buffer->setBufferData(GL_SHADER_STORAGE_BUFFER, OutIndex, GL_UNSIGNED_INT, maxIndexCount, GL_STREAM_DRAW, NULL);
	buffer->useAs(OutIndex, GL_ELEMENT_ARRAY_BUFFER);
	buffer->setBufferData(GL_SHADER_STORAGE_BUFFER, IndirectIndex, GL_ONE, sizeof(Indirect), GL_STREAM_DRAW, indirectBuffer);
	buffer->useAs(IndirectIndex, GL_DRAW_INDIRECT_BUFFER);
	buffer->unuse();
	return buffer;
}

RenderBuffer* TerrainDrawcall::createSSBuffers() {
	ChunkBuffer* chunkBuffer = (ChunkBuffer*)malloc(chunkCount * sizeof(ChunkBuffer));
	memset(chunkBuffer, 0, chunkCount * sizeof(ChunkBuffer));
	uint* indexBuffer = (uint*)malloc(chunkCount * CHUNK_INDEX_COUNT * sizeof(uint));

	for (int i = 0; i < chunkCount; ++i) {
		Chunk* chunk = mesh->chunks[i];
		chunk->genBounding(data->vertexBuffer, CHUNK_INDEX_COUNT);
		chunkBuffer[i].data[0] = chunk->boundCenter.x;
		chunkBuffer[i].data[1] = chunk->boundCenter.y;
		chunkBuffer[i].data[2] = chunk->boundCenter.z;
		chunkBuffer[i].data[4] = chunk->boundSize.x;
		chunkBuffer[i].data[5] = chunk->boundSize.y;
		chunkBuffer[i].data[6] = chunk->boundSize.z;
		for (uint j = 0; j < CHUNK_INDEX_COUNT; ++j)
			indexBuffer[i * CHUNK_INDEX_COUNT + j] = chunk->indices[j];
	}

	RenderBuffer* buffer = new RenderBuffer(2, false);
	buffer->setBufferData(GL_SHADER_STORAGE_BUFFER, ChunkIndex, GL_ONE, chunkCount * sizeof(ChunkBuffer), GL_STATIC_DRAW, chunkBuffer);
	buffer->setBufferData(GL_SHADER_STORAGE_BUFFER, InputIndex, GL_UNSIGNED_INT, chunkCount * CHUNK_INDEX_COUNT, GL_STATIC_DRAW, indexBuffer);

	free(chunkBuffer);
	free(indexBuffer);
	return buffer;
}

TerrainDrawcall::~TerrainDrawcall() {
	free(indirectBuffer);
	delete ssBuffer;
}

void TerrainDrawcall::update(Camera* camera, Render* render, RenderState* state) {
	indirectBuffer->count = 0; // Refresh index count
	dataBuffer->updateBufferMap(GL_DRAW_INDIRECT_BUFFER, IndirectIndex, sizeof(Indirect), indirectBuffer);

	ssBuffer->setShaderBase(ChunkIndex, 1);
	ssBuffer->setShaderBase(InputIndex, 2);
	dataBuffer->setShaderBase(IndirectIndex, 3);
	dataBuffer->setShaderBase(OutIndex, 4);

	render->useShader(state->shaderCompute);
	state->shaderCompute->setMatrix4("viewProjectMatrix", camera->viewProjectMatrix);
	
	glDispatchCompute(chunkCount, 1, 1); // Update per chunk, check chunk cull
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	UnbindShaderBuffers(1, 4);

	/*
	dataBuffer->readBufferData(GL_DRAW_INDIRECT_BUFFER, IndirectIndex, sizeof(Indirect), indirectBuffer);
	printf("terrain index cnt: %d\n", indirectBuffer->count);
	//*/
}

void TerrainDrawcall::draw(Render* render, RenderState* state, Shader* shader) {
	bool drawLine = render->drawLine;
	if (render->getDebugTerrain()) {
		render->setDrawLine(true);
		MaterialManager::materials->useMaterialBuffer(1);
	}

	if (frame < state->delay) frame++;
	else {
		dataBuffer->use();
		render->useShader(shader);
		glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0);
	}

	render->setDrawLine(drawLine);
}
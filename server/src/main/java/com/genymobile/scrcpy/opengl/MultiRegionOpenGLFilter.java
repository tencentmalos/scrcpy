package com.genymobile.scrcpy.opengl;

import com.genymobile.scrcpy.device.Size;
import com.genymobile.scrcpy.util.AffineMatrix;

import android.opengl.GLES11Ext;
import android.opengl.GLES20;

import java.nio.FloatBuffer;

public class MultiRegionOpenGLFilter implements OpenGLFilter {

    private int program;
    private FloatBuffer vertexBuffer;
    private FloatBuffer texCoordsBuffer;

    private final float[] userMatrix1;
    private final float[] userMatrix2;

    private final int viewport1X;
    private final int viewport1Y;
    private final int viewport1Width;
    private final int viewport1Height;

    private final int viewport2X;
    private final int viewport2Y;
    private final int viewport2Width;
    private final int viewport2Height;

    private int vertexPosLoc;
    private int texCoordsInLoc;

    private int texLoc;
    private int texMatrixLoc;
    private int userMatrixLoc;

    public MultiRegionOpenGLFilter(AffineMatrix transform1, Size region1Size, AffineMatrix transform2, Size region2Size) { // Removed concatenateVertically
        if (transform1 != null) {
            this.userMatrix1 = transform1.to4x4();
        } else {
            // If transform1 is null, use an identity matrix (or a matrix that scales to zero if region1Size is zero)
            this.userMatrix1 = AffineMatrix.IDENTITY.to4x4();
        }
        this.viewport1X = 0;
        this.viewport1Y = 0;
        this.viewport1Width = region1Size != null ? region1Size.getWidth() : 0;
        this.viewport1Height = region1Size != null ? region1Size.getHeight() : 0;

        if (transform2 != null) {
            this.userMatrix2 = transform2.to4x4();
        } else {
            this.userMatrix2 = AffineMatrix.IDENTITY.to4x4();
        }

        // Always horizontal concatenation
        this.viewport2X = (this.viewport1Width > 0) ? this.viewport1Width : 0;
        this.viewport2Y = 0;
        this.viewport2Width = (region2Size != null && region2Size.getWidth() > 0 && region2Size.getHeight() > 0) ? region2Size.getWidth() : 0;
        this.viewport2Height = (region2Size != null && region2Size.getWidth() > 0 && region2Size.getHeight() > 0) ? region2Size.getHeight() : 0;
    }

    @Override
    public void init() throws OpenGLException {
        // Vertex shader code (same as AffineOpenGLFilter)
        // @formatter:off
        String vertexShaderCode = "#version 100\n"
                + "attribute vec4 vertex_pos;\n"      // Standard quad vertices (-1 to 1)
                + "attribute vec4 tex_coords_in;\n"   // Standard tex coords (0 to 1 for input texture)
                + "varying vec2 tex_coords;\n"
                + "uniform mat4 tex_matrix;\n"        // From SurfaceTexture, transforms tex_coords_in
                + "uniform mat4 user_matrix;\n"       // User-defined (crop/rotation specific to a region)
                + "void main() {\n"
                + "    gl_Position = vertex_pos;\n" // Output directly to clip space (viewport handles positioning on screen)
                + "    tex_coords = (tex_matrix * user_matrix * tex_coords_in).xy;\n" // Calculate final texture coordinates
                + "}";

        // Fragment shader code (same as AffineOpenGLFilter)
        String fragmentShaderCode = "#version 100\n"
                + "#extension GL_OES_EGL_image_external : require\n"
                + "precision highp float;\n"
                + "uniform samplerExternalOES tex;\n"
                + "varying vec2 tex_coords;\n"
                + "void main() {\n"
                + "    if (tex_coords.x >= 0.0 && tex_coords.x <= 1.0\n" // Clamp to valid texture region
                + "            && tex_coords.y >= 0.0 && tex_coords.y <= 1.0) {\n"
                + "        gl_FragColor = texture2D(tex, tex_coords);\n"
                + "    } else {\n"
                + "        gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);\n" // Transparent black outside
                + "    }\n"
                + "}\n"; // Added trailing newline for consistency, moved comment
        // @formatter:on

        program = GLUtils.createProgram(vertexShaderCode, fragmentShaderCode);
        if (program == 0) {
            throw new OpenGLException("Cannot create OpenGL program for MultiRegionOpenGLFilter");
        }

        // Standard vertices for a full-screen quad (or full-viewport quad)
        float[] vertices = {
                -1, -1, // Bottom-left
                 1, -1, // Bottom-right
                -1,  1, // Top-left
                 1,  1, // Top-right
        };

        // Standard texture coordinates for sampling the input texture
        float[] texCoords = {
                0, 0, // Bottom-left
                1, 0, // Bottom-right
                0, 1, // Top-left
                1, 1, // Top-right
        };

        vertexBuffer = GLUtils.createFloatBuffer(vertices);
        texCoordsBuffer = GLUtils.createFloatBuffer(texCoords);

        vertexPosLoc = GLES20.glGetAttribLocation(program, "vertex_pos");
        assert vertexPosLoc != -1 : "Failed to get 'vertex_pos' location";

        texCoordsInLoc = GLES20.glGetAttribLocation(program, "tex_coords_in");
        assert texCoordsInLoc != -1 : "Failed to get 'tex_coords_in' location";

        texLoc = GLES20.glGetUniformLocation(program, "tex");
        assert texLoc != -1 : "Failed to get 'tex' uniform location";

        texMatrixLoc = GLES20.glGetUniformLocation(program, "tex_matrix");
        assert texMatrixLoc != -1 : "Failed to get 'tex_matrix' uniform location";

        userMatrixLoc = GLES20.glGetUniformLocation(program, "user_matrix");
        assert userMatrixLoc != -1 : "Failed to get 'user_matrix' uniform location";
    }

    @Override
    public void draw(int textureId, float[] texMatrix) {
        GLES20.glUseProgram(program);
        GLUtils.checkGlError("glUseProgram");

        GLES20.glEnableVertexAttribArray(vertexPosLoc);
        GLUtils.checkGlError("glEnableVertexAttribArray vertexPosLoc");
        GLES20.glEnableVertexAttribArray(texCoordsInLoc);
        GLUtils.checkGlError("glEnableVertexAttribArray texCoordsInLoc");

        GLES20.glVertexAttribPointer(vertexPosLoc, 2, GLES20.GL_FLOAT, false, 0, vertexBuffer);
        GLUtils.checkGlError("glVertexAttribPointer vertexPosLoc");
        GLES20.glVertexAttribPointer(texCoordsInLoc, 2, GLES20.GL_FLOAT, false, 0, texCoordsBuffer);
        GLUtils.checkGlError("glVertexAttribPointer texCoordsInLoc");

        GLES20.glActiveTexture(GLES20.GL_TEXTURE0);
        GLUtils.checkGlError("glActiveTexture");
        GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, textureId);
        GLUtils.checkGlError("glBindTexture");
        GLES20.glUniform1i(texLoc, 0);
        GLUtils.checkGlError("glUniform1i texLoc");

        GLES20.glUniformMatrix4fv(texMatrixLoc, 1, false, texMatrix, 0);
        GLUtils.checkGlError("glUniformMatrix4fv texMatrixLoc");

        // Clear the entire area (managed by OpenGLRunner's viewport) once
        // GLES20.glClear(GLES20.GL_COLOR_BUFFER_BIT); // Assuming OpenGLRunner clears its viewport before calling this.
        // GLUtils.checkGlError("glClear");


        // Draw Region 1
        if (viewport1Width > 0 && viewport1Height > 0) {
            GLES20.glViewport(viewport1X, viewport1Y, viewport1Width, viewport1Height);
            GLUtils.checkGlError("glViewport region 1");
            GLES20.glUniformMatrix4fv(userMatrixLoc, 1, false, userMatrix1, 0);
            GLUtils.checkGlError("glUniformMatrix4fv userMatrix1");
            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
            GLUtils.checkGlError("glDrawArrays region 1");
        }

        // Draw Region 2
        if (viewport2Width > 0 && viewport2Height > 0) {
            GLES20.glViewport(viewport2X, viewport2Y, viewport2Width, viewport2Height);
            GLUtils.checkGlError("glViewport region 2");
            GLES20.glUniformMatrix4fv(userMatrixLoc, 1, false, userMatrix2, 0);
            GLUtils.checkGlError("glUniformMatrix4fv userMatrix2");
            GLES20.glDrawArrays(GLES20.GL_TRIANGLE_STRIP, 0, 4);
            GLUtils.checkGlError("glDrawArrays region 2");
        }

        GLES20.glDisableVertexAttribArray(vertexPosLoc);
        GLES20.glDisableVertexAttribArray(texCoordsInLoc);
        GLES20.glBindTexture(GLES11Ext.GL_TEXTURE_EXTERNAL_OES, 0);
    }

    @Override
    public void release() {
        GLES20.glDeleteProgram(program);
        GLUtils.checkGlError("glDeleteProgram");
    }
}
